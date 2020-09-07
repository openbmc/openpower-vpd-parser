#include "config.h"

#include "defines.hpp"
#include "ipz_parser.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "parser_factory.hpp"
#include "utils.hpp"
#include "vpd_exceptions.hpp"

#include <assert.h>
#include <ctype.h>

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cstdarg>
#include <exception>
#include <filesystem>
#include <fstream>
#include <gpiod.hpp>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>

using namespace std;
using namespace openpower::vpd;
using namespace CLI;
using namespace vpd::keyword::parser;
using namespace openpower::vpd::constants;
namespace fs = filesystem;
using json = nlohmann::json;
using namespace openpower::vpd::parser::factory;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::memory::parser;
using namespace openpower::vpd::parser::interface;
using namespace openpower::vpd::exceptions;
using namespace phosphor::logging;

static const deviceTreeMap deviceTreeSystemTypeMap = {
    {RAINIER_2U, "conf-aspeed-bmc-ibm-rainier.dtb"},
    {RAINIER_4U, "conf-aspeed-bmc-ibm-rainier-4u.dtb"},
    {RAINIER_1S4U, "conf-aspeed-bmc-ibm-rainier-1s4u.dtb"},
    {EVEREST, "conf-aspeed-bmc-ibm-everest.dtb"}};

/**
 * @brief Returns the power state for chassis0
 */
static auto getPowerState()
{
    // TODO: How do we handle multiple chassis?
    string powerState{};
    auto bus = sdbusplus::bus::new_default();
    auto properties =
        bus.new_method_call("xyz.openbmc_project.State.Chassis",
                            "/xyz/openbmc_project/state/chassis0",
                            "org.freedesktop.DBus.Properties", "Get");
    properties.append("xyz.openbmc_project.State.Chassis");
    properties.append("CurrentPowerState");
    auto result = bus.call(properties);
    if (!result.is_method_error())
    {
        variant<string> val;
        result.read(val);
        if (auto pVal = get_if<string>(&val))
        {
            powerState = *pVal;
        }
    }
    cout << "Power state is: " << powerState << endl;
    return powerState;
}

/**
 * @brief Expands location codes
 */
static auto expandLocationCode(const string& unexpanded, const Parsed& vpdMap,
                               bool isSystemVpd)
{
    auto expanded{unexpanded};
    static constexpr auto SYSTEM_OBJECT = "/system/chassis/motherboard";
    static constexpr auto VCEN_IF = "com.ibm.ipzvpd.VCEN";
    static constexpr auto VSYS_IF = "com.ibm.ipzvpd.VSYS";
    size_t idx = expanded.find("fcs");
    try
    {
        if (idx != string::npos)
        {
            string fc{};
            string se{};
            if (isSystemVpd)
            {
                const auto& fcData = vpdMap.at("VCEN").at("FC");
                const auto& seData = vpdMap.at("VCEN").at("SE");
                fc = string(fcData.data(), fcData.size());
                se = string(seData.data(), seData.size());
            }
            else
            {
                fc = readBusProperty(SYSTEM_OBJECT, VCEN_IF, "FC");
                se = readBusProperty(SYSTEM_OBJECT, VCEN_IF, "SE");
            }

            // TODO: See if ND0 can be placed in the JSON
            expanded.replace(idx, 3, fc.substr(0, 4) + ".ND0." + se);
        }
        else
        {
            idx = expanded.find("mts");
            if (idx != string::npos)
            {
                string mt{};
                string se{};
                if (isSystemVpd)
                {
                    const auto& mtData = vpdMap.at("VSYS").at("TM");
                    const auto& seData = vpdMap.at("VSYS").at("SE");
                    mt = string(mtData.data(), mtData.size());
                    se = string(seData.data(), seData.size());
                }
                else
                {
                    mt = readBusProperty(SYSTEM_OBJECT, VSYS_IF, "TM");
                    se = readBusProperty(SYSTEM_OBJECT, VSYS_IF, "SE");
                }

                replace(mt.begin(), mt.end(), '-', '.');
                expanded.replace(idx, 3, mt + "." + se);
            }
        }
    }
    catch (exception& e)
    {
        cerr << "Failed to expand location code with exception: " << e.what()
             << "\n";
    }
    return expanded;
}

/**
 * @brief Populate FRU specific interfaces.
 *
 * This is a common method which handles both
 * ipz and keyword specific interfaces thus,
 * reducing the code redundancy.
 * @param[in] map - Reference to the innermost keyword-value map.
 * @param[in] preIntrStr - Reference to the interface string.
 * @param[out] interfaces - Reference to interface map.
 */
template <typename T>
static void populateFruSpecificInterfaces(const T& map,
                                          const string& preIntrStr,
                                          inventory::InterfaceMap& interfaces)
{
    inventory::PropertyMap prop;

    for (const auto& kwVal : map)
    {
        vector<uint8_t> vec(kwVal.second.begin(), kwVal.second.end());

        auto kw = kwVal.first;

        if (kw[0] == '#')
        {
            kw = string("PD_") + kw[1];
        }
        else if (isdigit(kw[0]))
        {
            kw = string("N_") + kw;
        }
        prop.emplace(move(kw), move(vec));
    }

    interfaces.emplace(preIntrStr, move(prop));
}

/**
 * @brief Populate Interfaces.
 *
 * This method populates common and extra interfaces to dbus.
 * @param[in] js - json object
 * @param[out] interfaces - Reference to interface map
 * @param[in] vpdMap - Reference to the parsed vpd map.
 * @param[in] isSystemVpd - Denotes whether we are collecting the system VPD.
 */
template <typename T>
static void populateInterfaces(const nlohmann::json& js,
                               inventory::InterfaceMap& interfaces,
                               const T& vpdMap, bool isSystemVpd)
{
    for (const auto& ifs : js.items())
    {
        string inf = ifs.key();
        inventory::PropertyMap props;

        for (const auto& itr : ifs.value().items())
        {
            const string& busProp = itr.key();

            if (itr.value().is_boolean())
            {
                props.emplace(busProp, itr.value().get<bool>());
            }
            else if (itr.value().is_string())
            {
                if constexpr (is_same<T, Parsed>::value)
                {
                    if (busProp == "LocationCode" &&
                        inf == "com.ibm.ipzvpd.Location")
                    {
                        auto prop = expandLocationCode(
                            itr.value().get<string>(), vpdMap, isSystemVpd);
                        props.emplace(busProp, prop);
                    }
                    else
                    {
                        props.emplace(busProp, itr.value().get<string>());
                    }
                }
                else
                {
                    props.emplace(busProp, itr.value().get<string>());
                }
            }
            else if (itr.value().is_object())
            {
                const string& rec = itr.value().value("recordName", "");
                const string& kw = itr.value().value("keywordName", "");
                const string& encoding = itr.value().value("encoding", "");

                if constexpr (is_same<T, Parsed>::value)
                {
                    if (!rec.empty() && !kw.empty() && vpdMap.count(rec) &&
                        vpdMap.at(rec).count(kw))
                    {
                        auto encoded =
                            encodeKeyword(vpdMap.at(rec).at(kw), encoding);
                        props.emplace(busProp, encoded);
                    }
                }
                else if constexpr (is_same<T, KeywordVpdMap>::value)
                {
                    if (!kw.empty() && vpdMap.count(kw))
                    {
                        auto prop =
                            string(vpdMap.at(kw).begin(), vpdMap.at(kw).end());
                        auto encoded = encodeKeyword(prop, encoding);
                        props.emplace(busProp, encoded);
                    }
                }
            }
        }
        interfaces.emplace(inf, move(props));
    }
}

static Binary getVpdDataInVector(const nlohmann::json& js, const string& file)
{
    uint32_t offset = 0;
    // check if offset present?
    for (const auto& item : js["frus"][file])
    {
        if (item.find("offset") != item.end())
        {
            offset = item["offset"];
        }
    }

    // TODO: Figure out a better way to get max possible VPD size.
    Binary vpdVector;
    vpdVector.resize(65504);
    ifstream vpdFile;
    vpdFile.open(file, ios::binary);

    vpdFile.seekg(offset, ios_base::cur);
    vpdFile.read(reinterpret_cast<char*>(&vpdVector[0]), 65504);
    vpdVector.resize(vpdFile.gcount());

    return vpdVector;
}

/** This API will be called at the end of VPD collection to perform any post
 * actions.
 *
 * @param[in] json - json object
 * @param[in] file - eeprom file path
 */
static void postFailAction(const nlohmann::json& json, const string& file)
{
    if ((json["frus"][file].at(0)).find("postActionFail") ==
        json["frus"][file].at(0).end())
    {
        return;
    }

    uint8_t pinValue = 0;
    string pinName;

    for (const auto& postAction :
         (json["frus"][file].at(0))["postActionFail"].items())
    {
        if (postAction.key() == "pin")
        {
            pinName = postAction.value();
        }
        else if (postAction.key() == "value")
        {
            // Get the value to set
            pinValue = postAction.value();
        }
    }

    cout << "Setting GPIO: " << pinName << " to " << (int)pinValue << endl;

    try
    {
        gpiod::line outputLine = gpiod::find_line(pinName);

        if (!outputLine)
        {
            cout << "Couldn't find output line:" << pinName
                 << " on GPIO. Skipping...\n";

            return;
        }
        outputLine.request(
            {"Disable line", ::gpiod::line_request::DIRECTION_OUTPUT, 0},
            pinValue);
    }
    catch (system_error&)
    {
        cerr << "Failed to set post-action GPIO" << endl;
    }
}

/** Performs any pre-action needed to get the FRU setup for collection.
 *
 * @param[in] json - json object
 * @param[in] file - eeprom file path
 */
static void preAction(const nlohmann::json& json, const string& file)
{
    if ((json["frus"][file].at(0)).find("preAction") ==
        json["frus"][file].at(0).end())
    {
        return;
    }

    uint8_t pinValue = 0;
    string pinName;

    for (const auto& postAction :
         (json["frus"][file].at(0))["preAction"].items())
    {
        if (postAction.key() == "pin")
        {
            pinName = postAction.value();
        }
        else if (postAction.key() == "value")
        {
            // Get the value to set
            pinValue = postAction.value();
        }
    }

    cout << "Setting GPIO: " << pinName << " to " << (int)pinValue << endl;
    try
    {
        gpiod::line outputLine = gpiod::find_line(pinName);

        if (!outputLine)
        {
            cout << "Couldn't find output line:" << pinName
                 << " on GPIO. Skipping...\n";

            return;
        }
        outputLine.request(
            {"FRU pre-action", ::gpiod::line_request::DIRECTION_OUTPUT, 0},
            pinValue);
    }
    catch (system_error&)
    {
        cerr << "Failed to set pre-action GPIO" << endl;
        return;
    }

    // Now bind the device
    string bind = json["frus"][file].at(0).value("bind", "");
    cout << "Binding device " << bind << endl;
    string bindCmd = string("echo \"") + bind +
                     string("\" > /sys/bus/i2c/drivers/at24/bind");
    cout << bindCmd << endl;
    executeCmd(bindCmd);

    // Check if device showed up (test for file)
    if (!fs::exists(file))
    {
        cout << "EEPROM " << file << " does not exist. Take failure action"
             << endl;
        // If not, then take failure postAction
        postFailAction(json, file);
    }
}

/**
 * @brief Prime the Inventory
 * Prime the inventory by populating only the location code,
 * type interface and the inventory object for the frus
 * which are not system vpd fru.
 *
 * @param[in] jsObject - Reference to vpd inventory json object
 * @param[in] vpdMap -  Reference to the parsed vpd map
 *
 * @returns Map of items in extraInterface.
 */
template <typename T>
inventory::ObjectMap primeInventory(const nlohmann::json& jsObject,
                                    const T& vpdMap)
{
    inventory::ObjectMap objects;

    for (auto& itemFRUS : jsObject["frus"].items())
    {
        // Take pre actions
        preAction(jsObject, itemFRUS.key());
        for (auto& itemEEPROM : itemFRUS.value())
        {
            inventory::InterfaceMap interfaces;
            auto isSystemVpd = itemEEPROM.value("isSystemVpd", false);
            inventory::Object object(itemEEPROM.at("inventoryPath"));

            if (!isSystemVpd && !itemEEPROM.value("noprime", false))
            {
                inventory::PropertyMap presProp;
                presProp.emplace("Present", false);
                interfaces.emplace("xyz.openbmc_project.Inventory.Item",
                                   move(presProp));

                if (itemEEPROM.find("extraInterfaces") != itemEEPROM.end())
                {
                    for (const auto& eI : itemEEPROM["extraInterfaces"].items())
                    {
                        inventory::PropertyMap props;
                        if (eI.key() ==
                            openpower::vpd::constants::LOCATION_CODE_INF)
                        {
                            if constexpr (std::is_same<T, Parsed>::value)
                            {
                                for (auto& lC : eI.value().items())
                                {
                                    auto propVal = expandLocationCode(
                                        lC.value().get<string>(), vpdMap, true);

                                    props.emplace(move(lC.key()),
                                                  move(propVal));
                                    interfaces.emplace(move(eI.key()),
                                                       move(props));
                                }
                            }
                        }
                        else if (eI.key().find("Inventory.Item.") !=
                                 string::npos)
                        {
                            interfaces.emplace(move(eI.key()), move(props));
                        }
                    }
                }
                objects.emplace(move(object), move(interfaces));
            }
        }
    }
    return objects;
}

/**
 * @brief This API executes command to set environment variable
 *        And then reboot the system
 * @param[in] key   -env key to set new value
 * @param[in] value -value to set.
 */
void setEnvAndReboot(const string& key, const string& value)
{
    // set env and reboot and break.
    executeCmd("/sbin/fw_setenv", key, value);
    log<level::INFO>("Rebooting BMC to pick up new device tree");
    // make dbus call to reboot
    auto bus = sdbusplus::bus::new_default_system();
    auto method = bus.new_method_call(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "Reboot");
    bus.call_noreply(method);
}

/*
 * @brief This API checks for env var fitconfig.
 *        If not initialised OR updated as per the current system type,
 *        update this env var and reboot the system.
 *
 * @param[in] systemType IM kwd in vpd tells about which system type it is.
 * */
void setDevTreeEnv(const string& systemType)
{
    string newDeviceTree;

    if (deviceTreeSystemTypeMap.find(systemType) !=
        deviceTreeSystemTypeMap.end())
    {
        newDeviceTree = deviceTreeSystemTypeMap.at(systemType);
    }

    string readVarValue;
    bool envVarFound = false;

    vector<string> output = executeCmd("/sbin/fw_printenv");
    for (const auto& entry : output)
    {
        size_t pos = entry.find("=");
        string key = entry.substr(0, pos);
        if (key != "fitconfig")
        {
            continue;
        }

        envVarFound = true;
        if (pos + 1 < entry.size())
        {
            readVarValue = entry.substr(pos + 1);
            if (readVarValue.find(newDeviceTree) != string::npos)
            {
                // fitconfig is Updated. No action needed
                break;
            }
        }
        // set env and reboot and break.
        // setEnvAndReboot(key, newDeviceTree);
        exit(0);
    }

    // check If env var Not found
    if (!envVarFound)
    {
        // setEnvAndReboot("fitconfig", newDeviceTree);
    }
}

/**
 * @brief API to call VPD manager to write VPD to EEPROM.
 * @param[in] Object path.
 * @param[in] record to be updated.
 * @param[in] keyword to be updated.
 * @param[in] keyword data to be updated
 */
void updateHardware(const string& objectName, const string& recName,
                    const string& kwdName, const Binary& data)
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto properties =
            bus.new_method_call(BUSNAME, OBJPATH, IFACE, "WriteKeyword");
        properties.append(
            static_cast<sdbusplus::message::object_path>(objectName));
        properties.append(recName);
        properties.append(kwdName);
        properties.append(data);
        bus.call(properties);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::string what =
            "VPDManager WriteKeyword api failed for inventory path " +
            objectName;
        what += " record " + recName;
        what += " keyword " + kwdName;
        what += " with bus error = " + std::string(e.what());

        // map to hold additional data in case of logging pel
        PelAdditionalData additionalData{};
        additionalData.emplace("CALLOUT_INVENTORY_PATH", objectName);
        additionalData.emplace("DESCRIPTION", what);
        createPEL(additionalData, errIntfForBusFailure);
    }
}

/**
 * @brief API to check if we need to restore system VPD
 * This functionality is only applicable for IPZ VPD data.
 * @param[in] vpdMap - IPZ vpd map
 * @param[in] objectPath - Object path for the FRU
 * @return EEPROMs with records and keywords updated at standby
 */
std::vector<RestoredEeproms> restoreSystemVPD(Parsed& vpdMap,
                                              const string& objectPath)
{
    // the list of keywords for VSYS record is as per the S0 system. Should be
    // updated for another type of systems
    static std::unordered_map<std::string, std::vector<std::string>> svpdKwdMap{
        {"VSYS", {"BR", "TM", "SE", "SU", "RB"}},
        {"VCEN", {"FC", "SE"}},
        {"LXR0", {"LX"}}};

    // vector to hold all the EEPROMs updated at standby
    std::vector<RestoredEeproms> updatedEeproms = {};

    for (const auto& systemRecKwdPair : svpdKwdMap)
    {
        auto it = vpdMap.find(systemRecKwdPair.first);

        // check if record is found in map we got by parser
        if (it != vpdMap.end())
        {
            const auto& kwdListForRecord = systemRecKwdPair.second;
            for (const auto& keyword : kwdListForRecord)
            {
                DbusPropertyMap& kwdValMap = it->second;
                auto iterator = kwdValMap.find(keyword);

                if (iterator != kwdValMap.end())
                {
                    string& kwdValue = iterator->second;

                    // check bus data
                    const string& recordName = systemRecKwdPair.first;
                    const string& busValue = readBusProperty(
                        objectPath, ipzVpdInf + recordName, keyword);

                    if (busValue.find_first_not_of(' ') != string::npos)
                    {
                        if (kwdValue.find_first_not_of(' ') != string::npos)
                        {
                            // both the data are present, check for mismatch
                            if (busValue != kwdValue)
                            {
                                string errMsg = "VPD data mismatch on cache "
                                                "and hardware for record: ";
                                errMsg += (*it).first;
                                errMsg += " and keyword: ";
                                errMsg += keyword;

                                // data mismatch
                                PelAdditionalData additionalData;
                                additionalData.emplace("CALLOUT_INVENTORY_PATH",
                                                       objectPath);

                                additionalData.emplace("DESCRIPTION", errMsg);

                                createPEL(additionalData, errIntfForInvalidVPD);
                            }
                        }
                        else
                        {
                            // implies hardware data is blank
                            // update the map
                            Binary busData(busValue.begin(), busValue.end());

                            updatedEeproms.push_back(std::make_tuple(
                                objectPath, recordName, keyword, busData));
                        }

                        // update the map as well, so that cache data is not
                        // updated as blank while populating VPD map on Dbus in
                        // populateDBus Api
                        kwdValue = busValue;
                        continue;
                    }
                    else if (kwdValue.find_first_not_of(' ') == string::npos)
                    {
                        string errMsg = "VPD is blank on both cache and "
                                        "hardware for record: ";
                        errMsg += (*it).first;
                        errMsg += " and keyword: ";
                        errMsg += keyword;
                        errMsg += ". SSR need to update hardware VPD.";

                        // both the data are blanks, log PEL
                        PelAdditionalData additionalData;
                        additionalData.emplace("CALLOUT_INVENTORY_PATH",
                                               objectPath);

                        additionalData.emplace("DESCRIPTION", errMsg);

                        // log PEL TODO: Block IPL
                        createPEL(additionalData, errIntfForBlankSystemVPD);
                        continue;
                    }
                }
            }
        }
    }

    return updatedEeproms;
}

/**
 * @brief Populate Dbus.
 * This method invokes all the populateInterface functions
 * and notifies PIM about dbus object.
 * @param[in] vpdMap - Either IPZ vpd map or Keyword vpd map based on the
 * input.
 * @param[in] js - Inventory json object
 * @param[in] filePath - Path of the vpd file
 * @param[in] preIntrStr - Interface string
 */
template <typename T>
static void populateDbus(T& vpdMap, nlohmann::json& js, const string& filePath)
{
    inventory::InterfaceMap interfaces;
    inventory::ObjectMap objects;
    inventory::PropertyMap prop;

    // map to hold all the keywords whose value has been changed at standby
    vector<RestoredEeproms> updatedEeproms = {};

    bool isSystemVpd = false;
    for (const auto& item : js["frus"][filePath])
    {
        const auto& objectPath = item["inventoryPath"];
        sdbusplus::message::object_path object(objectPath);
        isSystemVpd = item.value("isSystemVpd", false);

        // Populate the VPD keywords and the common interfaces only if we
        // are asked to inherit that data from the VPD, else only add the
        // extraInterfaces.
        if (item.value("inherit", true))
        {
            if constexpr (is_same<T, Parsed>::value)
            {
                if (isSystemVpd)
                {
                    std::vector<std::string> interfaces = {
                        motherBoardInterface};
                    // call mapper to check for object path creation
                    MapperResponse subTree =
                        getObjectSubtreeForInterfaces(pimPath, 0, interfaces);

                    // Skip system vpd restore if object path is not generated
                    // for motherboard, Implies first boot.
                    if (subTree.size() != 0)
                    {
                        assert(
                            (subTree.find(pimPath + std::string(objectPath)) !=
                             subTree.end()));

                        updatedEeproms = restoreSystemVPD(vpdMap, objectPath);
                    }
                    else
                    {
                        log<level::ERR>("No object path found");
                    }
                }

                // Each record in the VPD becomes an interface and all
                // keyword within the record are properties under that
                // interface.
                for (const auto& record : vpdMap)
                {
                    populateFruSpecificInterfaces(
                        record.second, ipzVpdInf + record.first, interfaces);
                }
            }
            else if constexpr (is_same<T, KeywordVpdMap>::value)
            {
                populateFruSpecificInterfaces(vpdMap, kwdVpdInf, interfaces);
            }
            if (js.find("commonInterfaces") != js.end())
            {
                populateInterfaces(js["commonInterfaces"], interfaces, vpdMap,
                                   isSystemVpd);
            }
        }
        else
        {
            // Check if we have been asked to inherit specific record(s)
            if constexpr (is_same<T, Parsed>::value)
            {
                if (item.find("copyRecords") != item.end())
                {
                    for (const auto& record : item["copyRecords"])
                    {
                        const string& recordName = record;
                        if (vpdMap.find(recordName) != vpdMap.end())
                        {
                            populateFruSpecificInterfaces(
                                vpdMap.at(recordName), ipzVpdInf + recordName,
                                interfaces);
                        }
                    }
                }
            }
        }
        if (item.value("inheritEI", true))
        {
            // Populate interfaces and properties that are common to every FRU
            // and additional interface that might be defined on a per-FRU
            // basis.
            if (item.find("extraInterfaces") != item.end())
            {
                populateInterfaces(item["extraInterfaces"], interfaces, vpdMap,
                                   isSystemVpd);
            }
        }
        objects.emplace(move(object), move(interfaces));
    }

    if (isSystemVpd)
    {
        vector<uint8_t> imVal;
        if constexpr (is_same<T, Parsed>::value)
        {
            auto property = vpdMap.find("VSBP");
            if (property != vpdMap.end())
            {
                auto value = (property->second).find("IM");
                if (value != (property->second).end())
                {
                    copy(value->second.begin(), value->second.end(),
                         back_inserter(imVal));
                }
            }
        }

        fs::path target;
        fs::path link = INVENTORY_JSON_SYM_LINK;

        ostringstream oss;
        for (auto& i : imVal)
        {
            oss << setw(2) << setfill('0') << hex << static_cast<int>(i);
        }
        string imValStr = oss.str();

        if ((imValStr == RAINIER_4U) || // 4U
            (imValStr == RAINIER_1S4U))
        {
            target = INVENTORY_JSON_4U;
        }
        else if (imValStr == RAINIER_2U) // 2U
        {
            target = INVENTORY_JSON_2U;
        }
        else if (imValStr == EVEREST)
        {
            target = INVENTORY_JSON_EVEREST;
        }

        // Create the directory for hosting the symlink
        fs::create_directories(VPD_FILES_PATH);
        // unlink the symlink previously created (if any)
        remove(INVENTORY_JSON_SYM_LINK);
        // create a new symlink based on the system
        fs::create_symlink(target, link);

        // Reloading the json
        ifstream inventoryJson(link);
        auto js = json::parse(inventoryJson);
        inventoryJson.close();

        inventory::ObjectMap primeObject = primeInventory(js, vpdMap);
        objects.insert(primeObject.begin(), primeObject.end());

        // set the U-boot environment variable for device-tree
        setDevTreeEnv(imValStr);

        // if system VPD has been restored at standby, update the EEPROM
        for (const auto& item : updatedEeproms)
        {
            updateHardware(get<0>(item), get<1>(item), get<2>(item),
                           get<3>(item));
        }
    }

    // Notify PIM
    inventory::callPIM(move(objects));
}

int main(int argc, char** argv)
{
    int rc = 0;
    string file{};
    json js{};

    // map to hold additional data in case of logging pel
    PelAdditionalData additionalData{};

    // this is needed to hold base fru inventory path in case there is ECC or
    // vpd exception while parsing the file
    std::string baseFruInventoryPath = {};

    try
    {
        std::cout << "1" << std::endl;
        App app{"ibm-read-vpd - App to read IPZ format VPD, parse it and store "
                "in DBUS"};
        string file{};

        app.add_option("-f, --file", file, "File containing VPD (IPZ/KEYWORD)")
            ->required();

        CLI11_PARSE(app, argc, argv);

        auto jsonToParse = INVENTORY_JSON_DEFAULT;

        // If the symlink exists, it means it has been setup for us, switch the
        // path
        if (fs::exists(INVENTORY_JSON_SYM_LINK))
        {
            jsonToParse = INVENTORY_JSON_SYM_LINK;
        }

        std::cout << "2" << std::endl;
        // Make sure that the file path we get is for a supported EEPROM
        ifstream inventoryJson(jsonToParse);
        if (!inventoryJson)
        {
            throw(
                (VpdJsonException("Failed to access Json path", jsonToParse)));
        }

        try
        {
            js = json::parse(inventoryJson);
        }
        catch (json::parse_error& ex)
        {
            throw((VpdJsonException("Json parsing failed", jsonToParse)));
        }

        if ((js.find("frus") == js.end()) ||
            (js["frus"].find(file) == js["frus"].end()))
        {
            cout << "Device path not in JSON, ignoring" << endl;
            return 0;
        }

        if (!fs::exists(file))
        {
            cout << "Device path: " << file
                 << " does not exist. Spurious udev event? Exiting." << endl;
            return 0;
        }

        baseFruInventoryPath = js["frus"][file][0]["inventoryPath"];
        // Check if we can read the VPD file based on the power state
        if (js["frus"][file].at(0).value("powerOffOnly", false))
        {
            if ("xyz.openbmc_project.State.Chassis.PowerState.On" ==
                getPowerState())
            {
                cout << "This VPD cannot be read when power is ON" << endl;
                return 0;
            }
        }
        std::cout << "3" << std::endl;
        try
        {
            Binary vpdVector = getVpdDataInVector(js, file);
            ParserInterface* parser = ParserFactory::getParser(move(vpdVector));

            std::cout << "4" << std::endl;
            variant<KeywordVpdMap, Store> parseResult;
            parseResult = parser->parse();
            std::cout << "5" << std::endl;
            if (auto pVal = get_if<Store>(&parseResult))
            {
                populateDbus(pVal->getVpdMap(), js, file);
            }
            else if (auto pVal = get_if<KeywordVpdMap>(&parseResult))
            {
                populateDbus(*pVal, js, file);
            }
            std::cout << "6" << std::endl;

            // release the parser object
            ParserFactory::freeParser(parser);
        }
        catch (exception& e)
        {
            std::cout << "Exception from here" << std::endl;
            postFailAction(js, file);
            throw e;
        }
    }
    catch (const VpdJsonException& ex)
    {
        additionalData.emplace("JSON_PATH", ex.getJsonPath());
        additionalData.emplace("DESCRIPTION", ex.what());
        createPEL(additionalData, errIntfForJsonFailure);

        cerr << ex.what() << "\n";
        rc = -1;
    }
    catch (const VpdEccException& ex)
    {
        additionalData.emplace("DESCRIPTION", "ECC check failed");
        additionalData.emplace("CALLOUT_INVENTORY_PATH",
                               INVENTORY_PATH + baseFruInventoryPath);
        createPEL(additionalData, errIntfForEccCheckFail);

        cerr << ex.what() << "\n";
        rc = -1;
    }
    catch (const VpdDataException& ex)
    {
        additionalData.emplace("DESCRIPTION", "Invalid VPD data");
        additionalData.emplace("CALLOUT_INVENTORY_PATH",
                               INVENTORY_PATH + baseFruInventoryPath);
        createPEL(additionalData, errIntfForInvalidVPD);

        cerr << ex.what() << "\n";
        rc = -1;
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
        rc = -1;
    }

    return rc;
}
