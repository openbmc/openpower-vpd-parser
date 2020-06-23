#include "config.h"

#include "defines.hpp"
#include "ipz_parser.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "parser_factory.hpp"
#include "utils.hpp"

#include <ctype.h>

#include <CLI/CLI.hpp>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <gpiod.hpp>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>

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

            // TODO: See if ND1 can be placed in the JSON
            expanded.replace(idx, 3, fc.substr(0, 4) + ".ND1." + se);
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

Binary getVpdDataInVector(nlohmann::json& js, const string& filePath)
{
    uint32_t offset = 0;
    // check if offset present?
    for (const auto& item : js["frus"][filePath])
    {
        if (item.find("offset") != item.end())
        {
            offset = item["offset"];
        }
    }
    char buf[2048];
    ifstream vpdFile;
    vpdFile.rdbuf()->pubsetbuf(buf, sizeof(buf));
    vpdFile.open(filePath, ios::binary);
    vpdFile.seekg(offset, ios_base::cur);

    // Read 64KB data content of the binary file into a vector
    Binary tmpVector((istreambuf_iterator<char>(vpdFile)),
                     istreambuf_iterator<char>());

    vector<unsigned char>::const_iterator first = tmpVector.begin();
    vector<unsigned char>::const_iterator last = tmpVector.begin() + NEXT_64_KB;

    if (distance(first, last) < static_cast<int>(tmpVector.size()))
    {
        Binary vpdVector(first, last);
        return vpdVector;
    }

    return tmpVector;
}

/** @brief This API will be called before vpd collection starts.
 *         Based on the presence state of FRU, it set/unset it's bus line
 *
 * @param[in] js - json object
 */
void configGPIO(const nlohmann::json& json)
{
    uint8_t setOrReset = 0;
    uint8_t polarity = 0;
    string presencePinName, outputPinName;

    inventory::ObjectMap objects;

    for (const auto& eachFRU : json["frus"].items())
    {
        for (const auto& eachInventory : eachFRU.value())
        {
            if (eachInventory.find("preAction") != eachInventory.end())
            {
                // Get the pin No and polarity to know the "presence"
                for (const auto& presStatus : eachInventory["presence"].items())
                {
                    if (presStatus.key() == "pin")
                    {
                        presencePinName = presStatus.value();
                    }
                    else if (presStatus.key() == "polarity")
                    {
                        if (presStatus.value() == "ACTIVE_HIGH")
                        {
                            polarity = 1;
                        }
                    }
                }

                uint8_t gpioData = 0;
                gpiod::line presenceLine = gpiod::find_line(presencePinName);

                if (!presenceLine)
                {
                    cout << "configGPIO: couldn't find presence line:"
                         << presencePinName << " on GPIO, for Inventory path- "
                         << eachInventory["inventoryPath"] << ". Skipping...\n";
                    continue;
                }

                presenceLine.request({"Read the presence line",
                                      gpiod::line_request::DIRECTION_INPUT, 0});

                gpioData = presenceLine.get_value();

                // Take action, if it is 1 && polarity HIGH OR if it is 0 &&
                // polarity LOW
                if (!(gpioData ^ polarity))
                {
                    for (const auto& preAction :
                         eachInventory["preAction"].items())
                    {
                        if (preAction.key() == "pin")
                        {
                            outputPinName = preAction.value();
                        }
                        else if (preAction.key() == "value")
                        {
                            setOrReset = preAction.value();
                        }
                    }

                    gpiod::line outputLine = gpiod::find_line(outputPinName);

                    if (!outputLine)
                    {
                        cout << "configGPIO: couldn't find output line:"
                             << outputPinName
                             << " on GPIO, for Inventory path- "
                             << eachInventory["inventoryPath"]
                             << ". Skipping...\n";
                        continue;
                    }

                    outputLine.request({"FRU present: set the output pin",
                                        gpiod::line_request::DIRECTION_OUTPUT,
                                        0},
                                       setOrReset);

                    // bind the driver
                    string str = "echo ";
                    string devNameAddr = eachInventory["devAddress"];
                    string driverType = eachInventory["driverType"];
                    string busType = eachInventory["busType"];

                    str = str + devNameAddr + " > /sys/bus/" + busType +
                          "/drivers/" + driverType + "/bind";

                    system(str.c_str());
                }
            }
        }
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
        for (auto& itemEEPROM : itemFRUS.value())
        {
            inventory::InterfaceMap interfaces;
            auto isSystemVpd = itemEEPROM.value("isSystemVpd", false);
            inventory::Object object(itemEEPROM.at("inventoryPath"));

            if (!isSystemVpd)
            {
                for (const auto& eI : itemEEPROM["extraInterfaces"].items())
                {
                    inventory::PropertyMap props;
                    if (eI.key() == LOCATION_CODE_INF)
                    {
                        if constexpr (is_same<T, Parsed>::value)
                        {
                            for (auto& lC : eI.value().items())
                            {
                                auto propVal =
                                    expandLocationCode(lC.value().get<string>(),
                                                       vpdMap, isSystemVpd);

                                props.emplace(move(lC.key()), move(propVal));
                                interfaces.emplace(move(eI.key()), move(props));
                            }
                        }
                    }
                    else if (eI.key().find("Inventory.Item.") != string::npos)
                    {
                        interfaces.emplace(move(eI.key()), move(props));
                    }
                }
                objects.emplace(move(object), move(interfaces));
            }
        }
    }
    return objects;
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

        throw runtime_error(what);
    }

    return;
}

/**
 * @brief API to create PEL entry
 * @param[in] objectPath - Object path for the FRU, to be sent as additional
 * data while creating PEL
 */
void createPEL(const std::string& objPath)
{
    try
    {
        // create PEL
        std::map<std::string, std::string> additionalData;
        auto bus = sdbusplus::bus::new_default();

        additionalData.emplace("CALLOUT_INVENTORY_PATH", objPath);

        std::string service =
            getService(bus, loggerObjectPath, loggerCreateInterface);
        auto method = bus.new_method_call(service.c_str(), loggerObjectPath,
                                          loggerCreateInterface, "Create");

        method.append(errIntfForBlankSystemVPD,
                      "xyz.openbmc_project.Logging.Entry.Level.Error",
                      additionalData);
        auto resp = bus.call(method);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        throw std::runtime_error(
            "Error in invoking D-Bus logging create interface to register PEL");
    }
}

/**
 * @brief API to check if we need to restore system VPD
 * @param[in] vpdMap - whild holds mapping of record and Kwd
 * @param[in] objectPath - Object path for the FRU
 */
template <typename T>
void restoreSystemVPD(T& vpdMap, const string& objectPath)
{
    static std::unordered_map<std::string, std::vector<std::string>> svpdKwdMap{
        {"VSYS",
         {"BR", "TM", "SE", "SU", "SG", "TN", "MN", "NN", "ID", "RG", "WN",
          "RB", "DR"}},
        {"VCEN", {"FC", "SE", "RG", "FC"}},
        {"LXR0", {"LX"}}};

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

                    // check if string has only ASCII spaces
                    if (kwdValue.find_first_not_of(' ') != string::npos)
                    {
                        // implies the data is not blank so continue with
                        // hardware data
                        continue;
                    }

                    const string& recordName = systemRecKwdPair.first;
                    const string& busValue = readBusProperty(
                        objectPath, ipzVpdInf + recordName, keyword);

                    if (busValue.find_first_not_of(' ') != string::npos)
                    {
                        // data is not blank on bus so convert to binary and
                        // write to EEPROM
                        Binary busData(busValue.begin(), busValue.end());

                        updateHardware(objectPath, recordName, keyword,
                                       busData);

                        // update the map as well
                        kwdValue = busValue;
                    }
                    else
                    {
                        // Log PEL data
                        createPEL(objectPath);
                        continue;
                    }
                }
            }
        }
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
        for (auto& itemEEPROM : itemFRUS.value())
        {
            inventory::InterfaceMap interfaces;
            auto isSystemVpd = itemEEPROM.value("isSystemVpd", false);
            inventory::Object object(itemEEPROM.at("inventoryPath"));

            if (!isSystemVpd)
            {
                for (const auto& eI : itemEEPROM["extraInterfaces"].items())
                {
                    inventory::PropertyMap props;
                    if (eI.key() == LOCATION_CODE_INF)
                    {
                        if constexpr (is_same<T, Parsed>::value)
                        {
                            for (auto& lC : eI.value().items())
                            {
                                auto propVal =
                                    expandLocationCode(lC.value().get<string>(),
                                                       vpdMap, isSystemVpd);

                                props.emplace(move(lC.key()), move(propVal));
                                interfaces.emplace(move(eI.key()), move(props));
                            }
                        }
                    }
                    else if (eI.key().find("Inventory.Item.") != string::npos)
                    {
                        interfaces.emplace(move(eI.key()), move(props));
                    }
                }
                objects.emplace(move(object), move(interfaces));
            }
        }
    }
    return objects;
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
static void populateDbus(const T& vpdMap, nlohmann::json& js,
                         const string& filePath) //, const string &preIntrStr) {
{
    inventory::InterfaceMap interfaces;
    inventory::ObjectMap objects;
    inventory::PropertyMap prop;

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
                    //                          imVal = value->second;
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
            if ((int)i / 10 == 0) // one digit number
            {
                oss << hex << 0;
            }
            oss << hex << static_cast<int>(i);
        }
        string imValStr = oss.str();

        if (imValStr == SYSTEM_4U) // 4U
        {
            target = INVENTORY_JSON_4U;
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

        // unlink the symlink which is created at build time
        remove(INVENTORY_JSON_SYM_LINK);
        // create a new symlink based on the system
        fs::create_symlink(target, link);

        // Reloading the json
        ifstream inventoryJson(link);
        auto js = json::parse(inventoryJson);
        inventoryJson.close();

        inventory::ObjectMap primeObject = primeInventory(js, vpdMap);
        objects.insert(primeObject.begin(), primeObject.end());

        // configure GPIO for all the inventory items, based on the JSON config.
        configGPIO(js);
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
                    //                          imVal = value->second;
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
            if ((int)i / 10 == 0) // one digit number
            {
                oss << hex << 0;
            }
            oss << hex << static_cast<int>(i);
        }
        string imValStr = oss.str();

        if (imValStr == SYSTEM_4U) // 4U
        {
            target = INVENTORY_JSON_4U;
        }

        else if (imValStr == SYSTEM_2U) // 2U
        {
            target = INVENTORY_JSON_2U;
        }

        // unlink the symlink which is created at build time
        remove(INVENTORY_JSON_SYM_LINK);
        // create a new symlink based on the system
        fs::create_symlink(target, link);

        // Reloading the json
        ifstream inventoryJson(link);
        auto js = json::parse(inventoryJson);
        inventoryJson.close();

        inventory::ObjectMap primeObject = primeInventory(js, vpdMap);
        objects.insert(primeObject.begin(), primeObject.end());
    }

    // Notify PIM
    inventory::callPIM(move(objects));
}

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        App app{"ibm-read-vpd - App to read IPZ format VPD, parse it and store "
                "in DBUS"};
        string file{};

        app.add_option("-f, --file", file, "File containing VPD (IPZ/KEYWORD)")
            ->required()
            ->check(ExistingFile);

        CLI11_PARSE(app, argc, argv);

        // Make sure that the file path we get is for a supported EEPROM
        ifstream inventoryJson(INVENTORY_JSON);
        auto js = json::parse(inventoryJson);

        if ((js.find("frus") == js.end()) ||
            (js["frus"].find(file) == js["frus"].end()))
        {
            cout << "Device path not in JSON, ignoring" << endl;
            return 0;
        }

        uint32_t offset = 0;
        // check if offset present?
        for (const auto& item : js["frus"][file])
        {
            if (item.find("offset") != item.end())
            {
                offset = item["offset"];
            }
        }
        char buf[2048];
        ifstream vpdFile;
        vpdFile.rdbuf()->pubsetbuf(buf, sizeof(buf));
        vpdFile.open(file, ios::binary);
        vpdFile.seekg(offset, std::ios_base::cur);

        // Read 64KB data content of the binary file into a vector
        Binary tmpVector((istreambuf_iterator<char>(vpdFile)),
                         istreambuf_iterator<char>());

        vector<unsigned char>::const_iterator first = tmpVector.begin();
        vector<unsigned char>::const_iterator last = tmpVector.begin() + 65536;

        Binary vpdVector(first, last);

        ParserInterface* parser =
            ParserFactory::getParser(std::move(vpdVector));

        variant<KeywordVpdMap, Store> parseResult;
        parseResult = parser->parse();

        if (auto pVal = get_if<Store>(&parseResult))
        {
            populateDbus(pVal->getVpdMap(), js, file);
        }
        else if (auto pVal = get_if<KeywordVpdMap>(&parseResult))
        {
            populateDbus(*pVal, js, file);
        }

        // release the parser object
        ParserFactory::freeParser(parser);
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
        rc = -1;
    }

    return rc;
}
