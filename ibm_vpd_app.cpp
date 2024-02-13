#include "config.h"

#include "common_utility.hpp"
#include "defines.hpp"
#include "editor_impl.hpp"
#include "ibm_vpd_utils.hpp"
#include "ipz_parser.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "parser_factory.hpp"
#include "vpd_exceptions.hpp"

#include <assert.h>
#include <ctype.h>

#include <CLI/CLI.hpp>
#include <boost/algorithm/string.hpp>
#include <gpiod.hpp>
#include <phosphor-logging/log.hpp>

#include <algorithm>
#include <cstdarg>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <thread>

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
using namespace openpower::vpd::manager::editor;

/**
 * @brief API declaration, Populate Dbus.
 *
 * This method invokes all the populateInterface functions
 * and notifies PIM about dbus object.
 *
 * @param[in] vpdMap - Either IPZ vpd map or Keyword vpd map based on the
 * input.
 * @param[in] js - Inventory json object
 * @param[in] filePath - Path of the vpd file
 * @param[in] preIntrStr - Interface string
 */
template <typename T>
static void populateDbus(T& vpdMap, nlohmann::json& js, const string& filePath);

/**
 * @brief Returns the BMC state
 */
static auto getBMCState()
{
    std::string bmcState;
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto properties = bus.new_method_call(
            "xyz.openbmc_project.State.BMC", "/xyz/openbmc_project/state/bmc0",
            "org.freedesktop.DBus.Properties", "Get");
        properties.append("xyz.openbmc_project.State.BMC");
        properties.append("CurrentBMCState");
        auto result = bus.call(properties);
        std::variant<std::string> val;
        result.read(val);
        if (auto pVal = std::get_if<std::string>(&val))
        {
            bmcState = *pVal;
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        // Ignore any error
        std::cerr << "Failed to get BMC state: " << e.what() << "\n";
    }
    return bmcState;
}

/**
 * @brief Check if the FRU is in the cache
 *
 * Checks if the FRU associated with the supplied D-Bus object path is already
 * on D-Bus. This can be used to test if a VPD collection is required for this
 * FRU. It uses the "xyz.openbmc_project.Inventory.Item, Present" property to
 * determine the presence of a FRU in the cache.
 *
 * @param objectPath - The D-Bus object path without the PIM prefix.
 * @return true if the object exists on D-Bus, false otherwise.
 */
static auto isFruInVpdCache(const std::string& objectPath)
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto invPath = std::string{pimPath} + objectPath;
        auto props = bus.new_method_call(
            "xyz.openbmc_project.Inventory.Manager", invPath.c_str(),
            "org.freedesktop.DBus.Properties", "Get");
        props.append("xyz.openbmc_project.Inventory.Item");
        props.append("Present");
        auto result = bus.call(props);
        std::variant<bool> present;
        result.read(present);
        if (auto pVal = std::get_if<bool>(&present))
        {
            return *pVal;
        }
        return false;
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cout << "FRU: " << objectPath << " not in D-Bus\n";
        // Assume not present in case of an error
        return false;
    }
}

/**
 * @brief Check if VPD recollection is needed for the given EEPROM
 *
 * Not all FRUs can be swapped at BMC ready state. This function does the
 * following:
 * -- Check if the FRU is marked as "pluggableAtStandby" OR
 *    "concurrentlyMaintainable". If so, return true.
 * -- Check if we are at BMC NotReady state. If we are, then return true.
 * -- Else check if the FRU is not present in the VPD cache (to cover for VPD
 *    force collection). If not found in the cache, return true.
 * -- Else return false.
 *
 * @param js - JSON Object.
 * @param filePath - The EEPROM file.
 * @return true if collection should be attempted, false otherwise.
 */
static auto needsRecollection(const nlohmann::json& js, const string& filePath)
{
    if (js["frus"][filePath].at(0).value("pluggableAtStandby", false) ||
        js["frus"][filePath].at(0).value("concurrentlyMaintainable", false))
    {
        return true;
    }
    if (getBMCState() == "xyz.openbmc_project.State.BMC.BMCState.NotReady")
    {
        return true;
    }
    if (!isFruInVpdCache(js["frus"][filePath].at(0).value("inventoryPath", "")))
    {
        return true;
    }
    return false;
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
    catch (const exception& e)
    {
        std::cerr << "Failed to expand location code with exception: "
                  << e.what() << "\n";
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
        auto kw = kwVal.first;

        if (kw[0] == '#')
        {
            kw = string("PD_") + kw[1];
        }
        else if (isdigit(kw[0]))
        {
            kw = string("N_") + kw;
        }
        if constexpr (is_same<T, KeywordVpdMap>::value)
        {
            if (auto keywordValue = get_if<Binary>(&kwVal.second))
            {
                Binary vec((*keywordValue).begin(), (*keywordValue).end());
                prop.emplace(move(kw), move(vec));
            }
            else if (auto keywordValue = get_if<std::string>(&kwVal.second))
            {
                Binary vec((*keywordValue).begin(), (*keywordValue).end());
                prop.emplace(move(kw), move(vec));
            }
            else if (auto keywordValue = get_if<size_t>(&kwVal.second))
            {
                if (kw == "MemorySizeInKB")
                {
                    inventory::PropertyMap memProp;
                    memProp.emplace(move(kw), ((*keywordValue)));
                    interfaces.emplace(
                        "xyz.openbmc_project.Inventory.Item.Dimm",
                        move(memProp));
                }
                else
                {
                    std::cerr << "Unknown Keyword[" << kw << "] found ";
                }
            }
            else
            {
                std::cerr << "Unknown Variant found ";
            }
        }
        else
        {
            Binary vec(kwVal.second.begin(), kwVal.second.end());
            prop.emplace(move(kw), move(vec));
        }
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
                if (busProp == "LocationCode" && inf == IBM_LOCATION_CODE_INF)
                {
                    std::string prop;
                    if constexpr (is_same<T, Parsed>::value)
                    {
                        // TODO deprecate the com.ibm interface later
                        prop = expandLocationCode(itr.value().get<string>(),
                                                  vpdMap, isSystemVpd);
                    }
                    else if constexpr (is_same<T, KeywordVpdMap>::value)
                    {
                        // Send empty Parsed object to expandLocationCode api.
                        prop = expandLocationCode(itr.value().get<string>(),
                                                  Parsed{}, false);
                    }
                    props.emplace(busProp, prop);
                    interfaces.emplace(XYZ_LOCATION_CODE_INF, props);
                    interfaces.emplace(IBM_LOCATION_CODE_INF, props);
                }
                else
                {
                    props.emplace(busProp, itr.value().get<string>());
                }
            }
            else if (itr.value().is_array())
            {
                try
                {
                    props.emplace(busProp, itr.value().get<Binary>());
                }
                catch (const nlohmann::detail::type_error& e)
                {
                    std::cerr << "Type exception: " << e.what() << "\n";
                    // Ignore any type errors
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
                        auto encoded = encodeKeyword(vpdMap.at(rec).at(kw),
                                                     encoding);
                        props.emplace(busProp, encoded);
                    }
                }
                else if constexpr (is_same<T, KeywordVpdMap>::value)
                {
                    if (!kw.empty() && vpdMap.count(kw))
                    {
                        if (auto kwValue = get_if<Binary>(&vpdMap.at(kw)))
                        {
                            auto prop = string((*kwValue).begin(),
                                               (*kwValue).end());

                            auto encoded = encodeKeyword(prop, encoding);

                            props.emplace(busProp, encoded);
                        }
                        else if (auto kwValue =
                                     get_if<std::string>(&vpdMap.at(kw)))
                        {
                            auto prop = string((*kwValue).begin(),
                                               (*kwValue).end());

                            auto encoded = encodeKeyword(prop, encoding);

                            props.emplace(busProp, encoded);
                        }
                        else if (auto uintValue =
                                     get_if<size_t>(&vpdMap.at(kw)))
                        {
                            props.emplace(busProp, *uintValue);
                        }
                        else
                        {
                            std::cerr << " Unknown Keyword [" << kw
                                      << "] Encountered";
                        }
                    }
                }
            }
            else if (itr.value().is_number())
            {
                // For now assume the value is a size_t.  In the future it would
                // be nice to come up with a way to get the type from the JSON.
                props.emplace(busProp, itr.value().get<size_t>());
            }
        }
        insertOrMerge(interfaces, inf, move(props));
    }
}

/**
 * @brief This API checks if this FRU is pcie_devices. If yes then it further
 *        checks whether it is PASS1 planar.
 */
static bool isThisPcieOnPass1planar(const nlohmann::json& js,
                                    const string& file)
{
    auto isThisPCIeDev = false;
    auto isPASS1 = false;

    // Check if it is a PCIE device
    if (js["frus"].find(file) != js["frus"].end())
    {
        if ((js["frus"][file].at(0).find("extraInterfaces") !=
             js["frus"][file].at(0).end()))
        {
            if (js["frus"][file].at(0)["extraInterfaces"].find(
                    "xyz.openbmc_project.Inventory.Item.PCIeDevice") !=
                js["frus"][file].at(0)["extraInterfaces"].end())
            {
                isThisPCIeDev = true;
            }
        }
    }

    if (isThisPCIeDev)
    {
        // Collect HW version and SystemType to know if it is PASS1 planar.
        auto bus = sdbusplus::bus::new_default();
        auto property1 = bus.new_method_call(
            INVENTORY_MANAGER_SERVICE,
            "/xyz/openbmc_project/inventory/system/chassis/motherboard",
            "org.freedesktop.DBus.Properties", "Get");
        property1.append("com.ibm.ipzvpd.VINI");
        property1.append("HW");
        auto result1 = bus.call(property1);
        inventory::Value hwVal;
        result1.read(hwVal);

        // SystemType
        auto property2 = bus.new_method_call(
            INVENTORY_MANAGER_SERVICE,
            "/xyz/openbmc_project/inventory/system/chassis/motherboard",
            "org.freedesktop.DBus.Properties", "Get");
        property2.append("com.ibm.ipzvpd.VSBP");
        property2.append("IM");
        auto result2 = bus.call(property2);
        inventory::Value imVal;
        result2.read(imVal);

        auto pVal1 = get_if<Binary>(&hwVal);
        auto pVal2 = get_if<Binary>(&imVal);

        if (pVal1 && pVal2)
        {
            auto hwVersion = *pVal1;
            auto systemType = *pVal2;

            // IM kw for Everest
            Binary everestSystem{80, 00, 48, 00};

            if (systemType == everestSystem)
            {
                if (hwVersion[1] < 21)
                {
                    isPASS1 = true;
                }
            }
            else if (hwVersion[1] < 2)
            {
                isPASS1 = true;
            }
        }
    }

    return (isThisPCIeDev && isPASS1);
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

    try
    {
        if (executePreAction(json, file))
        {
            if (json["frus"][file].at(0).find("devAddress") !=
                json["frus"][file].at(0).end())
            {
                // Now bind the device
                string bind = json["frus"][file].at(0).value("devAddress", "");
                std::cout << "Binding device " << bind << std::endl;
                string bindCmd = string("echo \"") + bind +
                                 string("\" > /sys/bus/i2c/drivers/at24/bind");
                std::cout << bindCmd << std::endl;
                executeCmd(bindCmd);

                // Check if device showed up (test for file)
                if (!fs::exists(file))
                {
                    std::cerr << "EEPROM " << file
                              << " does not exist. Take failure action"
                              << std::endl;
                    // If not, then take failure postAction
                    executePostFailAction(json, file);
                }
            }
            else
            {
                // missing required informations
                std::cerr << "VPD inventory JSON missing basic informations of "
                             "preAction "
                             "for this FRU : ["
                          << file << "]. Executing executePostFailAction."
                          << std::endl;

                // Take failure postAction
                executePostFailAction(json, file);
                return;
            }
        }
        else
        {
            // If the FRU is not there, clear the VINI/CCIN data.
            // Enity manager probes for this keyword to look for this
            // FRU, now if the data is persistent on BMC and FRU is
            // removed this can lead to ambiguity. Hence clearing this
            // Keyword if FRU is absent.
            const auto& invPath =
                json["frus"][file].at(0).value("inventoryPath", "");

            if (!invPath.empty())
            {
                inventory::ObjectMap pimObjMap{
                    {invPath, {{"com.ibm.ipzvpd.VINI", {{"CC", Binary{}}}}}}};

                common::utility::callPIM(move(pimObjMap));
            }
            else
            {
                throw std::runtime_error("Path empty in Json");
            }
        }
    }
    catch (const GpioException& e)
    {
        PelAdditionalData additionalData{};
        additionalData.emplace("DESCRIPTION", e.what());
        createPEL(additionalData, PelSeverity::WARNING, errIntfForGpioError,
                  nullptr);
    }
}

/**
 * @brief Fills the Decorator.AssetTag property into the interfaces map
 *
 * This function should only be called in cases where we did not find a JSON
 * symlink. A missing symlink in /var/lib will be considered as a factory reset
 * and this function will be used to default the AssetTag property.
 *
 * @param interfaces A possibly pre-populated map of inetrfaces to properties.
 * @param vpdMap A VPD map of the system VPD data.
 */
static void fillAssetTag(inventory::InterfaceMap& interfaces,
                         const Parsed& vpdMap)
{
    // Read the system serial number and MTM
    // Default asset tag is Server-MTM-System Serial
    inventory::Interface assetIntf{
        "xyz.openbmc_project.Inventory.Decorator.AssetTag"};
    inventory::PropertyMap assetTagProps;
    std::string defaultAssetTag =
        std::string{"Server-"} + getKwVal(vpdMap, "VSYS", "TM") +
        std::string{"-"} + getKwVal(vpdMap, "VSYS", "SE");
    assetTagProps.emplace("AssetTag", defaultAssetTag);
    insertOrMerge(interfaces, assetIntf, std::move(assetTagProps));
}

/**
 * @brief Set certain one time properties in the inventory
 * Use this function to insert the Functional and Enabled properties into the
 * inventory map. This function first checks if the object in question already
 * has these properties hosted on D-Bus, if the property is already there, it is
 * not modified, hence the name "one time". If the property is not already
 * present, it will be added to the map with a suitable default value (true for
 * Functional and Enabled)
 *
 * @param[in] object - The inventory D-Bus obejct without the inventory prefix.
 * @param[inout] interfaces - Reference to a map of inventory interfaces to
 * which the properties will be attached.
 */
static void setOneTimeProperties(const std::string& object,
                                 inventory::InterfaceMap& interfaces)
{
    auto bus = sdbusplus::bus::new_default();
    auto objectPath = INVENTORY_PATH + object;
    auto prop = bus.new_method_call("xyz.openbmc_project.Inventory.Manager",
                                    objectPath.c_str(),
                                    "org.freedesktop.DBus.Properties", "Get");
    prop.append("xyz.openbmc_project.State.Decorator.OperationalStatus");
    prop.append("Functional");
    try
    {
        auto result = bus.call(prop);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        // Treat as property unavailable
        inventory::PropertyMap prop;
        prop.emplace("Functional", true);
        interfaces.emplace(
            "xyz.openbmc_project.State.Decorator.OperationalStatus",
            move(prop));
    }
    prop = bus.new_method_call("xyz.openbmc_project.Inventory.Manager",
                               objectPath.c_str(),
                               "org.freedesktop.DBus.Properties", "Get");
    prop.append("xyz.openbmc_project.Object.Enable");
    prop.append("Enabled");
    try
    {
        auto result = bus.call(prop);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        // Treat as property unavailable
        inventory::PropertyMap prop;
        prop.emplace("Enabled", true);
        interfaces.emplace("xyz.openbmc_project.Object.Enable", move(prop));
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
            // Take pre actions if needed
            if (itemEEPROM.find("preAction") != itemEEPROM.end())
            {
                preAction(jsObject, itemFRUS.key());
            }

            inventory::InterfaceMap interfaces;
            inventory::Object object(itemEEPROM.at("inventoryPath"));

            if ((itemFRUS.key() != systemVpdFilePath) &&
                !itemEEPROM.value("noprime", false))
            {
                inventory::PropertyMap presProp;

                // Do not populate Present property for frus whose
                // synthesized=true. synthesized=true says the fru VPD is
                // synthesized and owned by a separate component.
                // In some cases, the FRU has its own VPD, but still a separate
                // application handles the FRU's presence. So VPD parser skips
                // populating Present property by checking the JSON flag,
                // "handlePresence".
                if (!itemEEPROM.value("synthesized", false))
                {
                    if (itemEEPROM.value("handlePresence", true))
                    {
                        presProp.emplace("Present", false);
                        interfaces.emplace("xyz.openbmc_project.Inventory.Item",
                                           presProp);
                    }
                }

                setOneTimeProperties(object, interfaces);
                if (itemEEPROM.find("extraInterfaces") != itemEEPROM.end())
                {
                    for (const auto& eI : itemEEPROM["extraInterfaces"].items())
                    {
                        inventory::PropertyMap props;
                        if (eI.key() == IBM_LOCATION_CODE_INF)
                        {
                            if constexpr (std::is_same<T, Parsed>::value)
                            {
                                for (auto& lC : eI.value().items())
                                {
                                    auto propVal = expandLocationCode(
                                        lC.value().get<string>(), vpdMap, true);

                                    props.emplace(move(lC.key()),
                                                  move(propVal));
                                    interfaces.emplace(XYZ_LOCATION_CODE_INF,
                                                       props);
                                    interfaces.emplace(move(eI.key()),
                                                       move(props));
                                }
                            }
                        }
                        else if (eI.key() ==
                                 "xyz.openbmc_project.Inventory.Item")
                        {
                            for (auto& val : eI.value().items())
                            {
                                if (val.key() == "PrettyName")
                                {
                                    presProp.emplace(val.key(),
                                                     val.value().get<string>());
                                }
                            }
                            // Use insert_or_assign here as we may already have
                            // inserted the present property only earlier in
                            // this function under this same interface.
                            interfaces.insert_or_assign(eI.key(),
                                                        move(presProp));
                        }
                        else
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
    // Init with default dtb
    string newDeviceTree = "conf-aspeed-bmc-ibm-rainier-p1.dtb";
    static const deviceTreeMap deviceTreeSystemTypeMap = {
        {RAINIER_2U, "conf-aspeed-bmc-ibm-rainier-p1.dtb"},
        {RAINIER_2U_V2, "conf-aspeed-bmc-ibm-rainier.dtb"},
        {RAINIER_4U, "conf-aspeed-bmc-ibm-rainier-4u-p1.dtb"},
        {RAINIER_4U_V2, "conf-aspeed-bmc-ibm-rainier-4u.dtb"},
        {RAINIER_1S4U, "conf-aspeed-bmc-ibm-rainier-1s4u.dtb"},
        {EVEREST, "conf-aspeed-bmc-ibm-everest.dtb"},
        {EVEREST_V2, "conf-aspeed-bmc-ibm-everest.dtb"},
        {BONNELL, "conf-aspeed-bmc-ibm-bonnell.dtb"}};

    if (deviceTreeSystemTypeMap.find(systemType) !=
        deviceTreeSystemTypeMap.end())
    {
        newDeviceTree = deviceTreeSystemTypeMap.at(systemType);
    }
    else
    {
        // System type not supported
        string err = "This System type not found/supported in dtb table " +
                     systemType +
                     ".Please check the HW and IM keywords in the system "
                     "VPD.Breaking...";

        // map to hold additional data in case of logging pel
        PelAdditionalData additionalData{};
        additionalData.emplace("DESCRIPTION", err);
        createPEL(additionalData, PelSeverity::WARNING,
                  errIntfForInvalidSystemType, nullptr);
        exit(-1);
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
        setEnvAndReboot(key, newDeviceTree);
        exit(0);
    }

    // check If env var Not found
    if (!envVarFound)
    {
        setEnvAndReboot("fitconfig", newDeviceTree);
    }
}

/**
 * @brief Parse the given EEPROM file.
 *
 * @param[in] vpdFilePath - Path of EEPROM file
 * @param[in] js- Reference to vpd inventory json object
 * @return Parsed VPD map
 */
std::variant<KeywordVpdMap, openpower::vpd::Store>
    parseVpdFile(const std::string& vpdFilePath, const nlohmann::json& js)
{
    uint32_t vpdStartOffset = 0;
    for (const auto& item : js["frus"][vpdFilePath])
    {
        if (item.find("offset") != item.end())
        {
            vpdStartOffset = item["offset"];
            break;
        }
    }

    Binary vpdVector = getVpdDataInVector(js, vpdFilePath);

    ParserInterface* parser = ParserFactory::getParser(
        vpdVector,
        (pimPath + js["frus"][vpdFilePath][0]["inventoryPath"]
                       .get_ref<const nlohmann::json::string_t&>()),
        vpdFilePath, vpdStartOffset);

    auto parseResult = parser->parse();

    // release the parser object
    ParserFactory::freeParser(parser);

    return parseResult;
}

/*
 * @brief This API retrieves the hardware backup in map
 *
 * @param[in] systemVpdBackupPath - The path that backs up the system VPD.
 * @param[in] backupVpdInvPath - FRU inventory path.
 * @param[in] js - JSON object.
 * @param[out] backupVpdMap - An IPZ VPD map containing the parsed backup VPD.
 *
 * */
void getBackupVpdInMap(const string& systemVpdBackupPath,
                       const string& backupVpdInvPath, const nlohmann::json& js,
                       Parsed& backupVpdMap)
{
    PelAdditionalData additionalData{};

    if (!fs::exists(systemVpdBackupPath))
    {
        string errorMsg = "Device path ";
        errorMsg += systemVpdBackupPath;
        errorMsg += " does not exist";

        additionalData.emplace("DESCRIPTION", errorMsg);

        additionalData.emplace("CALLOUT_INVENTORY_PATH",
                               INVENTORY_PATH + backupVpdInvPath);

        createPEL(additionalData, PelSeverity::ERROR, errIntfForStreamFail,
                  nullptr);
    }
    else
    {
        auto backupVpdParsedResult = parseVpdFile(systemVpdBackupPath, js);

        if (auto pVal = get_if<Store>(&backupVpdParsedResult))
        {
            backupVpdMap = pVal->getVpdMap();
        }
        else
        {
            std::cerr << "Invalid format of VPD in back up. Restore aborted."
                      << std::endl;
        }
    }
}

void updateVpdDataOnHw(const std::string& vpdFilePath, nlohmann::json& js,
                       const std::string& recName, const std::string& kwName,
                       const Binary& kwdData)
{
    const std::string& fruInvPath =
        js["frus"][vpdFilePath][0]["inventoryPath"]
            .get_ref<const nlohmann::json::string_t&>();

    EditorImpl edit(vpdFilePath, js, recName, kwName, fruInvPath);

    uint32_t offset = 0;
    // Setup offset, if any
    for (const auto& item : js["frus"][vpdFilePath])
    {
        if (item.find("offset") != item.end())
        {
            offset = item["offset"];
            break;
        }
    }

    // update keyword data on to EEPROM file
    // Note: Updating keyword data on cache is
    // handled via PIM Notify call hence passing
    // the updCache flag value as false here.
    edit.updateKeyword(kwdData, offset, false);
}

/**
 * @brief API to check if we need to restore system VPD
 * This functionality is only applicable for IPZ VPD data.

 * @param[in] vpdMap - IPZ vpd map
 * @param[in] objectPath - Object path for the FRU
 * @param[in] js - JSON Object
 * @param[in] isBackupOnCache - Denotes whether the backup is on cache/hardware
 */
void restoreSystemVPD(Parsed& vpdMap, const string& objectPath,
                      nlohmann::json& js, bool isBackupOnCache = true)
{
    std::string systemVpdBackupPath{};
    std::string backupVpdInvPath{};
    Parsed backupVpdMap{};

    if (!isBackupOnCache)
    {
        // Get the value of systemvpdBackupPath field from json
        systemVpdBackupPath = js["frus"][systemVpdFilePath].at(0).value(
            "systemVpdBackupPath", "");

        backupVpdInvPath = js["frus"][systemVpdBackupPath][0]["inventoryPath"]
                               .get_ref<const nlohmann::json::string_t&>();

        getBackupVpdInMap(systemVpdBackupPath, backupVpdInvPath, js,
                          backupVpdMap);

        if (backupVpdMap.empty())
        {
            std::cerr << "Backup VPD map is empty" << std::endl;
            return;
        }
    }

    for (const auto& systemRecKwdPair : svpdKwdMap)
    {
        const string& recordName = systemRecKwdPair.first;
        auto it = vpdMap.find(recordName);

        // check if record is found in map we got by parser
        if (it != vpdMap.end())
        {
            const auto& kwdListForRecord = systemRecKwdPair.second;
            for (const auto& keywordInfo : kwdListForRecord)
            {
                const auto keywordName = get<0>(keywordInfo);

                DbusPropertyMap& kwdValMap = it->second;
                auto iterator = kwdValMap.find(keywordName);

                if (iterator != kwdValMap.end())
                {
                    string& kwdValue = iterator->second;

                    std::string backupValue{};
                    const auto& defaultValue = get<1>(keywordInfo);
                    const auto& backupVpdRecName = get<4>(keywordInfo);
                    const auto& backupVpdKwName = get<5>(keywordInfo);

                    // If the 'isBackupOnCache' flag is false, we need
                    // to backup the systemVPD on the specified fru's eeprom
                    // path or restore it from the specified fru's eeprom path.
                    if (isBackupOnCache)
                    {
                        // check bus data
                        backupValue = readBusProperty(
                            objectPath, ipzVpdInf + recordName, keywordName);
                    }
                    else
                    {
                        backupValue = getKwVal(backupVpdMap, backupVpdRecName,
                                               backupVpdKwName);

                        if (backupValue.empty())
                        {
                            string errorMsg{};
                            if (backupVpdMap.find(backupVpdRecName) ==
                                backupVpdMap.end())
                            {
                                errorMsg = backupVpdRecName +
                                           " Record does not exist in "
                                           "the EEPROM file ";
                            }
                            else
                            {
                                errorMsg = backupVpdKwName +
                                           " Keyword not found or empty.";
                            }

                            errorMsg += systemVpdBackupPath;

                            PelAdditionalData additionalData;
                            additionalData.emplace("DESCRIPTION", errorMsg);

                            createPEL(additionalData, PelSeverity::ERROR,
                                      errIntfForInvalidVPD, nullptr);

                            continue;
                        }
                    }

                    Binary backupDataInBinary(backupValue.begin(),
                                              backupValue.end());

                    Binary kwdDataInBinary(kwdValue.begin(), kwdValue.end());

                    if (backupDataInBinary != defaultValue)
                    {
                        if (kwdDataInBinary != defaultValue)
                        {
                            // both the data are present, check for mismatch
                            if (backupValue != kwdValue)
                            {
                                string errMsg = "Mismatch found between backup "
                                                "and primary VPD for record: ";
                                errMsg += (*it).first;
                                errMsg += " and keyword: ";
                                errMsg += keywordName;

                                std::ostringstream busStream;
                                for (uint16_t byte : backupValue)
                                {
                                    busStream << std::setfill('0')
                                              << std::setw(2) << std::hex
                                              << "0x" << byte << " ";
                                }

                                std::ostringstream vpdStream;
                                for (uint16_t byte : kwdValue)
                                {
                                    vpdStream << std::setfill('0')
                                              << std::setw(2) << std::hex
                                              << "0x" << byte << " ";
                                }

                                // data mismatch
                                PelAdditionalData additionalData;

                                additionalData.emplace("DESCRIPTION", errMsg);
                                additionalData.emplace(
                                    "Value read from Backup: ",
                                    busStream.str());
                                additionalData.emplace(
                                    "Value read from Primary: ",
                                    vpdStream.str());

                                createPEL(additionalData, PelSeverity::WARNING,
                                          errIntfForVPDMismatch, nullptr);

                                if (!isBackupOnCache)
                                {
                                    // Backing up or restoring from a hardware
                                    // path does not requires copying the backup
                                    // data to the VPD map, as this will result
                                    // in a mismatch between the primary VPD and
                                    // its cache.
                                    continue;
                                }
                            }
                            else
                            {
                                // both the backup and primary data is
                                // non-default and same. Nothing needs to be
                                // done.
                                continue;
                            }
                        }

                        // If the backup is on the cache we need to copy the
                        // backup data to the VPD map to ensure there is no
                        // mimatch b/n them. So if backup data is not default,
                        // then irrespective of primary data(default or other
                        // than backup), copy the backup data to vpd map as we
                        // don't need to change the backup data in either case
                        // in the process of restoring system vpd.
                        kwdValue = backupValue;

                        // If the backup data is on the base panel the restoring
                        // of Backup VPD on to the system backplane VPD
                        // file is done here not through the VPD manager code
                        // path. This is to have the logic of restoring data on
                        // to the cache & hardware in the same code path.
                        if (!isBackupOnCache)
                        {
                            // copy backup VPD on to system backplane
                            // EEPROM  file.
                            updateVpdDataOnHw(systemVpdFilePath, js, recordName,
                                              keywordName, backupDataInBinary);
                        }
                    }
                    else if (kwdDataInBinary == defaultValue &&
                             get<2>(keywordInfo)) // Check isPELRequired is true
                    {
                        string errMsg = "Found default value on both backup "
                                        "and primary VPD for record: ";
                        errMsg += (*it).first;
                        errMsg += " and keyword: ";
                        errMsg += keywordName;
                        errMsg += ". Update primary VPD.";

                        // mfg default on both backup and primary, log PEL
                        PelAdditionalData additionalData;
                        additionalData.emplace("DESCRIPTION", errMsg);

                        createPEL(additionalData, PelSeverity::ERROR,
                                  errIntfForVPDDefault, nullptr);

                        continue;
                    }
                    else if ((kwdDataInBinary != defaultValue) &&
                             (!isBackupOnCache))
                    {
                        // update primary VPD on to backup VPD file
                        updateVpdDataOnHw(systemVpdBackupPath, js,
                                          backupVpdRecName, backupVpdKwName,
                                          kwdDataInBinary);

                        // copy primary VPD to backup VPD to publish on
                        // DBus
                        backupVpdMap.find(backupVpdRecName)
                            ->second.find(backupVpdKwName)
                            ->second = kwdValue;
                    }
                }
            }
        }
    }
}

/**
 * @brief This checks for is this FRU a processor
 *        And if yes, then checks for is this primary
 *
 * @param[in] js- vpd json to get the information about this FRU
 * @param[in] filePath- FRU vpd
 *
 * @return true/false
 */
bool isThisPrimaryProcessor(nlohmann::json& js, const string& filePath)
{
    bool isProcessor = false;
    bool isPrimary = false;

    for (const auto& item : js["frus"][filePath])
    {
        if (item.find("extraInterfaces") != item.end())
        {
            for (const auto& eI : item["extraInterfaces"].items())
            {
                if (eI.key().find("Inventory.Item.Cpu") != string::npos)
                {
                    isProcessor = true;
                }
            }
        }

        if (isProcessor)
        {
            string cpuType = item.value("cpuType", "");
            if (cpuType == "primary")
            {
                isPrimary = true;
            }
        }
    }

    return (isProcessor && isPrimary);
}

/**
 * @brief This finds DIMM vpd in vpd json and enables them by binding the device
 *        driver
 * @param[in] js- vpd json to iterate through and take action if it is DIMM
 */
void doEnableAllDimms(nlohmann::json& js)
{
    // iterate over each fru
    for (const auto& eachFru : js["frus"].items())
    {
        // skip the driver binding if eeprom already exists
        if (fs::exists(eachFru.key()))
        {
            continue;
        }

        for (const auto& eachInventory : eachFru.value())
        {
            if (eachInventory.find("extraInterfaces") != eachInventory.end())
            {
                for (const auto& eI : eachInventory["extraInterfaces"].items())
                {
                    if (eI.key().find("Inventory.Item.Dimm") != string::npos)
                    {
                        string dimmVpd = eachFru.key();
                        // fetch it from
                        // "/sys/bus/i2c/drivers/at24/414-0050/eeprom"

                        regex matchPatern("([0-9]+-[0-9]{4})");
                        smatch matchFound;
                        if (regex_search(dimmVpd, matchFound, matchPatern))
                        {
                            vector<string> i2cReg;
                            boost::split(i2cReg, matchFound.str(0),
                                         boost::is_any_of("-"));

                            // remove 0s from begining
                            const regex pattern("^0+(?!$)");
                            for (auto& i : i2cReg)
                            {
                                i = regex_replace(i, pattern, "");
                            }

                            // For ISDIMM which uses ee1004 driver
                            // the below is done
                            size_t stringFound = dimmVpd.find("ee1004");
                            if (stringFound != string::npos)
                            {
                                // echo ee1004 0x50 >
                                // /sys/bus/i2c/devices/i2c-110/new_device
                                string cmnd = "echo ee1004 0x" + i2cReg[1] +
                                              " > /sys/bus/i2c/devices/i2c-" +
                                              i2cReg[0] + "/new_device";
                                executeCmd(cmnd);
                            }
                            else if (i2cReg.size() == 2)
                            {
                                // echo 24c32 0x50 >
                                // /sys/bus/i2c/devices/i2c-16/new_device
                                string cmnd = "echo 24c32 0x" + i2cReg[1] +
                                              " > /sys/bus/i2c/devices/i2c-" +
                                              i2cReg[0] + "/new_device";
                                executeCmd(cmnd);
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief Check if the given CPU is an IO only chip.
 * The CPU is termed as IO, whose all of the cores are bad and can never be
 * used. Those CPU chips can be used for IO purpose like connecting PCIe devices
 * etc., The CPU whose every cores are bad, can be identified from the CP00
 * record's PG keyword, only if all of the 8 EQs' value equals 0xE7F9FF. (1EQ
 * has 4 cores grouped together by sharing its cache memory.)
 * @param [in] pgKeyword - PG Keyword of CPU.
 * @return true if the given cpu is an IO, false otherwise.
 */
static bool isCPUIOGoodOnly(const string& pgKeyword)
{
    const unsigned char io[] = {0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF, 0xE7, 0xF9,
                                0xFF, 0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF, 0xE7,
                                0xF9, 0xFF, 0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF};
    // EQ0 index (in PG keyword) starts at 97 (with offset starting from 0).
    // Each EQ carries 3 bytes of data. Totally there are 8 EQs. If all EQs'
    // value equals 0xE7F9FF, then the cpu has no good cores and its treated as
    // IO.
    if (memcmp(io, pgKeyword.data() + 97, 24) == 0)
    {
        return true;
    }

    // The CPU is not an IO
    return false;
}

/**
 * @brief Function to bring MUX out of idle state
 *
 *        This finds All the MUX defined in the system json and enables
 *        them by setting the holdidle parameter to 0.
 * @param[in] js- system json to iterate through and take action
 */
void doEnableAllMuxChips(const nlohmann::json& js)
{
    // Do we have the mandatory "muxes" section?
    if (js.find("muxes") != js.end())
    {
        std::cout << "Enabling all the MUX on the system " << std::endl;
        // iterate over each MUX detail and enable them
        for (const auto& item : js["muxes"])
        {
            if (item.find("holdidlepath") != item.end())
            {
                const std::string& holdidle = item["holdidlepath"];
                std::cout << "Setting holdidle state for " << holdidle
                          << "to 0 " << std::endl;
                string cmd = "echo 0 > " + holdidle;
                executeCmd(cmd);
            }
        }
        std::cout << "Completed enabling all the MUX on the system "
                  << std::endl;
    }
    else
    {
        std::cout << "No MUX was defined for the system" << std::endl;
    }
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
    string ccinFromVpd;

    bool isSystemVpd = (filePath == systemVpdFilePath);
    if constexpr (is_same<T, Parsed>::value)
    {
        ccinFromVpd = getKwVal(vpdMap, "VINI", "CC");
        transform(ccinFromVpd.begin(), ccinFromVpd.end(), ccinFromVpd.begin(),
                  ::toupper);

        if (isSystemVpd)
        {
            string mboardPath =
                js["frus"][filePath].at(0).value("inventoryPath", "");

            // Get the value of systemvpdBackupPath field from json
            const std::string& systemVpdBackupPath =
                js["frus"][filePath].at(0).value("systemVpdBackupPath", "");

            if (systemVpdBackupPath.empty())
            {
                std::vector<std::string> interfaces = {motherBoardInterface};
                // call mapper to check for object path creation
                MapperResponse subTree =
                    getObjectSubtreeForInterfaces(pimPath, 0, interfaces);

                // Attempt system VPD restore if we have a motherboard
                // object in the inventory.
                if ((subTree.size() != 0) &&
                    (subTree.find(pimPath + mboardPath) != subTree.end()))
                {
                    restoreSystemVPD(vpdMap, mboardPath, js);
                }
                else
                {
                    log<level::ERR>("No object path found");
                }
            }
            else
            {
                restoreSystemVPD(vpdMap, mboardPath, js, false);
            }
        }
        else
        {
            // check if it is processor vpd.
            auto isPrimaryCpu = isThisPrimaryProcessor(js, filePath);

            if (isPrimaryCpu)
            {
                auto ddVersion = getKwVal(vpdMap, "CRP0", "DD");

                auto chipVersion = atoi(ddVersion.substr(1, 2).c_str());

                if (chipVersion >= 2)
                {
                    doEnableAllDimms(js);
                    // Sleep for a few seconds to let the DIMM parses start
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(5s);
                }
            }
        }
    }

    auto processFactoryReset = false;

    if (isSystemVpd)
    {
        string systemJsonName{};
        if constexpr (is_same<T, Parsed>::value)
        {
            // pick the right system json
            systemJsonName = getSystemsJson(vpdMap);
        }

        fs::path target = systemJsonName;
        fs::path link = INVENTORY_JSON_SYM_LINK;

        // If the symlink does not exist, we treat that as a factory reset
        processFactoryReset = !fs::exists(INVENTORY_JSON_SYM_LINK);

        // Create the directory for hosting the symlink
        fs::create_directories(VPD_FILES_PATH);
        // unlink the symlink previously created (if any)
        remove(INVENTORY_JSON_SYM_LINK);
        // create a new symlink based on the system
        fs::create_symlink(target, link);

        // Reloading the json
        ifstream inventoryJson(link);
        js = json::parse(inventoryJson);
        inventoryJson.close();
    }

    for (const auto& item : js["frus"][filePath])
    {
        const auto& objectPath = item["inventoryPath"];
        sdbusplus::message::object_path object(objectPath);

        vector<string> ccinList;
        if (item.find("ccin") != item.end())
        {
            for (const auto& cc : item["ccin"])
            {
                string ccin = cc;
                transform(ccin.begin(), ccin.end(), ccin.begin(), ::toupper);
                ccinList.push_back(ccin);
            }
        }

        if (!ccinFromVpd.empty() && !ccinList.empty() &&
            (find(ccinList.begin(), ccinList.end(), ccinFromVpd) ==
             ccinList.end()))
        {
            continue;
        }

        if ((isSystemVpd) || (item.value("noprime", false)))
        {
            // Populate one time properties for the system VPD and its sub-frus
            // and for other non-primeable frus.
            // For the remaining FRUs, this will get handled as a part of
            // priming the inventory.
            setOneTimeProperties(objectPath, interfaces);
        }

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
        // Populate interfaces and properties that are common to every FRU
        // and additional interface that might be defined on a per-FRU
        // basis.
        if (item.find("extraInterfaces") != item.end())
        {
            populateInterfaces(item["extraInterfaces"], interfaces, vpdMap,
                               isSystemVpd);
            if constexpr (is_same<T, Parsed>::value)
            {
                if (item["extraInterfaces"].find(
                        "xyz.openbmc_project.Inventory.Item.Cpu") !=
                    item["extraInterfaces"].end())
                {
                    if (isCPUIOGoodOnly(getKwVal(vpdMap, "CP00", "PG")))
                    {
                        interfaces[invItemIntf]["PrettyName"] = "IO Module";
                    }
                }
            }
        }

        // embedded property(true or false) says whether the subfru is embedded
        // into the parent fru (or) not. VPD sets Present property only for
        // embedded frus. If the subfru is not an embedded FRU, the subfru may
        // or may not be physically present. Those non embedded frus will always
        // have Present=false irrespective of its physical presence or absence.
        // Eg: nvme drive in nvme slot is not an embedded FRU. So don't set
        // Present to true for such sub frus.
        // Eg: ethernet port is embedded into bmc card. So set Present to true
        // for such sub frus. Also donot populate present property for embedded
        // subfru which is synthesized. Currently there is no subfru which are
        // both embedded and synthesized. But still the case is handled here.
        if ((item.value("embedded", true)) &&
            (!item.value("synthesized", false)))
        {
            // Check if its required to handle presence for this FRU.
            if (item.value("handlePresence", true))
            {
                inventory::PropertyMap presProp;
                presProp.emplace("Present", true);
                insertOrMerge(interfaces, invItemIntf, move(presProp));
            }
        }

        if constexpr (is_same<T, Parsed>::value)
        {
            // Restore asset tag, if needed
            if (processFactoryReset && objectPath == "/system")
            {
                fillAssetTag(interfaces, vpdMap);
            }
        }

        objects.emplace(move(object), move(interfaces));
    }

    if (isSystemVpd)
    {
        inventory::ObjectMap primeObject = primeInventory(js, vpdMap);
        objects.insert(primeObject.begin(), primeObject.end());

        // set the U-boot environment variable for device-tree
        if constexpr (is_same<T, Parsed>::value)
        {
            setDevTreeEnv(fs::path(getSystemsJson(vpdMap)).filename());
        }
    }

    // Notify PIM
    common::utility::callPIM(move(objects));
}

int main(int argc, char** argv)
{
    int rc = 0;
    json js{};
    Binary vpdVector{};
    string file{};
    string driver{};
    // map to hold additional data in case of logging pel
    PelAdditionalData additionalData{};

    // this is needed to hold base fru inventory path in case there is ECC or
    // vpd exception while parsing the file
    std::string baseFruInventoryPath = {};

    // It holds the backup EEPROM file path for the system backplane's critical
    // data
    std::string systemVpdBackupPath{};

    // It holds the inventory path of backup EEPROM file
    std::string backupVpdInvPath{};

    bool isSystemVpd = false;

    // severity for PEL
    PelSeverity pelSeverity = PelSeverity::WARNING;

    try
    {
        App app{"ibm-read-vpd - App to read IPZ/Jedec format VPD, parse it and "
                "store it in DBUS"};

        app.add_option("-f, --file", file, "File containing VPD (IPZ/KEYWORD)")
            ->required();

        app.add_option("--driver", driver,
                       "Driver used by kernel (at24,at25,ee1004)")
            ->required();

        CLI11_PARSE(app, argc, argv);

        // PEL severity should be ERROR in case of any system VPD failure
        if (file == systemVpdFilePath)
        {
            pelSeverity = PelSeverity::ERROR;
            isSystemVpd = true;
        }

        // Check if input file is not empty.
        if ((file.empty()) || (driver.empty()))
        {
            std::cerr << "Encountered empty input parameter file [" << file
                      << "] driver [" << driver << "]" << std::endl;
            return 0;
        }

        // Check if currently supported driver or not
        if ((driver != at24driver) && (driver != at25driver) &&
            (driver != ee1004driver))
        {
            std::cerr << "The driver [" << driver << "] is not supported."
                      << std::endl;
            return 0;
        }

        auto jsonToParse = INVENTORY_JSON_DEFAULT;

        // If the symlink exists, it means it has been setup for us, switch the
        // path
        if (fs::exists(INVENTORY_JSON_SYM_LINK))
        {
            jsonToParse = INVENTORY_JSON_SYM_LINK;
        }

        // Make sure that the file path we get is for a supported EEPROM
        ifstream inventoryJson(jsonToParse);
        if (!inventoryJson)
        {
            throw(VpdJsonException("Failed to access Json path", jsonToParse));
        }

        try
        {
            js = json::parse(inventoryJson);
        }
        catch (const json::parse_error& ex)
        {
            throw(VpdJsonException("Json parsing failed", jsonToParse));
        }

        // Do we have the mandatory "frus" section?
        if (js.find("frus") == js.end())
        {
            throw(VpdJsonException("FRUs section not found in JSON",
                                   jsonToParse));
        }

        // Check if it's a udev path - patterned as(/ahb/ahb:apb/ahb:apb:bus@)
        if (file.find("/ahb:apb") != string::npos)
        {
            // Translate udev path to a generic /sys/bus/.. file path.
            udevToGenericPath(file, driver);

            if ((js["frus"].find(file) != js["frus"].end()) &&
                (file == systemVpdFilePath))
            {
                std::cout << "We have already collected system VPD, skiping."
                          << std::endl;
                return 0;
            }
        }

        // Enable all mux which are used for connecting to the i2c on the pcie
        // slots for pcie cards. These are not enabled by kernel due to an issue
        // seen with Castello cards, where the i2c line hangs on a probe.
        // To run it only once have kept it under System vpd check.
        // we need to run this on all BMC reboots so kept here
        if (file == systemVpdFilePath)
        {
            doEnableAllMuxChips(js);
        }

        if (file.empty())
        {
            std::cerr << "The EEPROM path <" << file << "> is not valid.";
            return 0;
        }
        if (js["frus"].find(file) == js["frus"].end())
        {
            std::cerr << "The EEPROM path [" << file
                      << "] is not found in the json." << std::endl;
            return 0;
        }

        if (!fs::exists(file))
        {
            std::cout << "Device path: " << file
                      << " does not exist. Spurious udev event? Exiting."
                      << std::endl;
            return 0;
        }

        // In case of system VPD it will already be filled, Don't have to
        // overwrite that.
        if (baseFruInventoryPath.empty())
        {
            baseFruInventoryPath = js["frus"][file][0]["inventoryPath"];
        }

        // Check if we can read the VPD file based on the power state
        // We skip reading VPD when the power is ON in two scenarios:
        // 1) The eeprom we are trying to read is that of the system VPD and the
        // JSON symlink is already setup (the symlink's existence tells us we
        // are not coming out of a factory reset)
        // 2) The JSON tells us that the FRU EEPROM cannot be
        // read when we are powered ON.
        if (js["frus"][file].at(0).value("powerOffOnly", false) ||
            (file == systemVpdFilePath && fs::exists(INVENTORY_JSON_SYM_LINK)))
        {
            if ("xyz.openbmc_project.State.Chassis.PowerState.On" ==
                getPowerState())
            {
                std::cout << "This VPD cannot be read when power is ON"
                          << std::endl;
                return 0;
            }
        }

        // Check if this VPD should be recollected at all
        if (!needsRecollection(js, file))
        {
            std::cout << "Skip VPD recollection for: " << file << std::endl;
            return 0;
        }

        try
        {
            variant<KeywordVpdMap, Store> parseResult;
            parseResult = parseVpdFile(file, js);

            if (isSystemVpd)
            {
                // Get the value of systemVpdBackupPath field from json
                systemVpdBackupPath = js["frus"][systemVpdFilePath].at(0).value(
                    "systemVpdBackupPath", "");

                if (!systemVpdBackupPath.empty())
                {
                    backupVpdInvPath =
                        js["frus"][systemVpdBackupPath][0]["inventoryPath"]
                            .get_ref<const nlohmann::json::string_t&>();
                }
            }

            if (auto pVal = get_if<Store>(&parseResult))
            {
                populateDbus(pVal->getVpdMap(), js, file);
            }
            else if (auto pVal = get_if<KeywordVpdMap>(&parseResult))
            {
                populateDbus(*pVal, js, file);
            }
        }
        catch (const exception& e)
        {
            if (!systemVpdBackupPath.empty())
            {
                file = systemVpdBackupPath;
                baseFruInventoryPath = backupVpdInvPath;
            }

            executePostFailAction(js, file);
            throw;
        }
    }
    catch (const VpdJsonException& ex)
    {
        additionalData.emplace("JSON_PATH", ex.getJsonPath());
        additionalData.emplace("DESCRIPTION", ex.what());
        createPEL(additionalData, pelSeverity, errIntfForJsonFailure, nullptr);

        std::cerr << ex.what() << "\n";
        rc = -1;
    }
    catch (const VpdEccException& ex)
    {
        additionalData.emplace("DESCRIPTION", "ECC check failed");
        additionalData.emplace("CALLOUT_INVENTORY_PATH",
                               INVENTORY_PATH + baseFruInventoryPath);
        createPEL(additionalData, pelSeverity, errIntfForEccCheckFail, nullptr);

        if (systemVpdBackupPath.empty())
        {
            dumpBadVpd(file, vpdVector);
        }

        std::cerr << ex.what() << "\n";
        rc = -1;
    }
    catch (const VpdDataException& ex)
    {
        if (isThisPcieOnPass1planar(js, file))
        {
            std::cout << "Pcie_device  [" << file
                      << "]'s VPD is not valid on PASS1 planar.Ignoring.\n";
            rc = 0;
        }
        else if (!(isPresent(js, file).value_or(true)))
        {
            std::cout << "FRU at: " << file
                      << " is not detected present. Ignore parser error.\n";
            rc = 0;
        }
        else
        {
            string errorMsg =
                "VPD file is either empty or invalid. Parser failed for [";
            errorMsg += file;
            errorMsg += "], with error = " + std::string(ex.what());

            additionalData.emplace("DESCRIPTION", errorMsg);
            additionalData.emplace("CALLOUT_INVENTORY_PATH",
                                   INVENTORY_PATH + baseFruInventoryPath);
            createPEL(additionalData, pelSeverity, errIntfForInvalidVPD,
                      nullptr);

            rc = -1;
        }
    }
    catch (const exception& e)
    {
        dumpBadVpd(file, vpdVector);
        std::cerr << e.what() << "\n";
        rc = -1;
    }

    return rc;
}
