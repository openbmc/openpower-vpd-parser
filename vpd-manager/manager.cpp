#include "config.h"

#include "manager.hpp"

#include "bios_handler.hpp"
#include "common_utility.hpp"
#include "editor_impl.hpp"
#include "gpioMonitor.hpp"
#include "ibm_vpd_utils.hpp"
#include "ipz_parser.hpp"
#include "reader_impl.hpp"
#include "vpd_exceptions.hpp"

#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace openpower::vpd::manager;
using namespace openpower::vpd::constants;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::manager::editor;
using namespace openpower::vpd::manager::reader;
using namespace std;
using namespace openpower::vpd::parser;
using namespace openpower::vpd::exceptions;
using namespace phosphor::logging;

namespace openpower
{
namespace vpd
{
namespace manager
{
Manager::Manager(sdbusplus::bus::bus&& bus, const char* busName,
                 const char* objPath, const char* /*iFace*/) :
    ServerObject<ManagerIface>(bus, objPath),
    _bus(std::move(bus)), _manager(_bus, objPath)
{
    _bus.request_name(busName);
}

void Manager::run()
{
    try
    {
        processJSON();
        listenHostState();
        listenAssetTag();

        // Create an instance of the BIOS handler
        BiosHandler biosHandler{_bus, *this};

        auto event = sdeventplus::Event::get_default();
        GpioMonitor gpioMon1(jsonFile, event);

        _bus.attach_event(event.get(), SD_EVENT_PRIORITY_IMPORTANT);
        cout << "VPD manager event loop started\n";
        event.loop();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
}

void Manager::listenHostState()
{
    static std::shared_ptr<sdbusplus::bus::match::match> hostState =
        std::make_shared<sdbusplus::bus::match::match>(
            _bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                "/xyz/openbmc_project/state/host0",
                "xyz.openbmc_project.State.Host"),
            [this](sdbusplus::message::message& msg) {
                hostStateCallBack(msg);
            });
}

void Manager::hostStateCallBack(sdbusplus::message::message& msg)
{
    if (msg.is_method_error())
    {
        std::cerr << "Error in reading signal " << std::endl;
    }

    Path object;
    PropertyMap propMap;
    msg.read(object, propMap);
    const auto itr = propMap.find("CurrentHostState");
    if (itr != propMap.end())
    {
        if (auto hostState = std::get_if<std::string>(&(itr->second)))
        {
            // implies system is moving from standby to power on state
            if (*hostState == "xyz.openbmc_project.State.Host.HostState."
                              "TransitioningToRunning")
            {
                // check and perfrom recollection for FRUs replaceable at
                // standby.
                performVPDRecollection();
                return;
            }
        }
        std::cerr << "Failed to read Host state" << std::endl;
    }
}

void Manager::listenAssetTag()
{
    static std::shared_ptr<sdbusplus::bus::match::match> assetMatcher =
        std::make_shared<sdbusplus::bus::match::match>(
            _bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                "/xyz/openbmc_project/inventory/system",
                "xyz.openbmc_project.Inventory.Decorator.AssetTag"),
            [this](sdbusplus::message::message& msg) {
                assetTagCallback(msg);
            });
}

void Manager::assetTagCallback(sdbusplus::message::message& msg)
{
    if (msg.is_method_error())
    {
        std::cerr << "Error in reading signal " << std::endl;
    }

    Path object;
    PropertyMap propMap;
    msg.read(object, propMap);
    const auto itr = propMap.find("AssetTag");
    if (itr != propMap.end())
    {
        if (auto assetTag = std::get_if<std::string>(&(itr->second)))
        {
            // Call Notify to persist the AssetTag
            inventory::ObjectMap objectMap = {
                {std::string{"/system"},
                 {{"xyz.openbmc_project.Inventory.Decorator.AssetTag",
                   {{"AssetTag", *assetTag}}}}}};

            common::utility::callPIM(std::move(objectMap));
        }
        else
        {
            std::cerr << "Failed to read asset tag" << std::endl;
        }
    }
}

void Manager::processJSON()
{
    std::ifstream json(INVENTORY_JSON_SYM_LINK, std::ios::binary);

    if (!json)
    {
        throw std::runtime_error("json file not found");
    }

    jsonFile = nlohmann::json::parse(json);
    if (jsonFile.find("frus") == jsonFile.end())
    {
        throw std::runtime_error("frus group not found in json");
    }

    const nlohmann::json& groupFRUS =
        jsonFile["frus"].get_ref<const nlohmann::json::object_t&>();
    for (const auto& itemFRUS : groupFRUS.items())
    {
        const std::vector<nlohmann::json>& groupEEPROM =
            itemFRUS.value().get_ref<const nlohmann::json::array_t&>();
        for (const auto& itemEEPROM : groupEEPROM)
        {
            bool isMotherboard = false;
            std::string redundantPath;

            if (itemEEPROM["extraInterfaces"].find(
                    "xyz.openbmc_project.Inventory.Item.Board.Motherboard") !=
                itemEEPROM["extraInterfaces"].end())
            {
                isMotherboard = true;
            }
            if (itemEEPROM.find("redundantEeprom") != itemEEPROM.end())
            {
                redundantPath = itemEEPROM["redundantEeprom"]
                                    .get_ref<const nlohmann::json::string_t&>();
            }
            frus.emplace(
                itemEEPROM["inventoryPath"]
                    .get_ref<const nlohmann::json::string_t&>(),
                std::make_tuple(itemFRUS.key(), redundantPath, isMotherboard));

            if (itemEEPROM["extraInterfaces"].find(IBM_LOCATION_CODE_INF) !=
                itemEEPROM["extraInterfaces"].end())
            {
                fruLocationCode.emplace(
                    itemEEPROM["extraInterfaces"][IBM_LOCATION_CODE_INF]
                              ["LocationCode"]
                                  .get_ref<const nlohmann::json::string_t&>(),
                    itemEEPROM["inventoryPath"]
                        .get_ref<const nlohmann::json::string_t&>());
            }

            if (itemEEPROM.value("replaceableAtStandby", false))
            {
                replaceableFrus.emplace_back(itemFRUS.key());
            }
        }
    }
}

void Manager::writeKeyword(const sdbusplus::message::object_path path,
                           const std::string recordName,
                           const std::string keyword, const Binary value)
{
    try
    {
        std::string objPath{path};
        // Strip any inventory prefix in path
        if (objPath.find(INVENTORY_PATH) == 0)
        {
            objPath = objPath.substr(sizeof(INVENTORY_PATH) - 1);
        }

        if (frus.find(objPath) == frus.end())
        {
            throw std::runtime_error("Inventory path not found");
        }

        inventory::Path vpdFilePath = std::get<0>(frus.find(objPath)->second);

        // instantiate editor class to update the data
        EditorImpl edit(vpdFilePath, jsonFile, recordName, keyword, objPath);

        uint32_t offset = 0;
        // Setup offset, if any
        for (const auto& item : jsonFile["frus"][vpdFilePath])
        {
            if (item.find("offset") != item.end())
            {
                offset = item["offset"];
                break;
            }
        }

        edit.updateKeyword(value, offset, true);

        // If we have a redundant EEPROM to update, then update just the EEPROM,
        // not the cache since that is already done when we updated the primary
        if (!std::get<1>(frus.find(objPath)->second).empty())
        {
            EditorImpl edit(std::get<1>(frus.find(objPath)->second), jsonFile,
                            recordName, keyword);
            edit.updateKeyword(value, offset, false);
        }

        // if it is a motehrboard FRU need to check for location expansion
        if (std::get<2>(frus.find(objPath)->second))
        {
            if (recordName == "VCEN" && (keyword == "FC" || keyword == "SE"))
            {
                edit.expandLocationCode("fcs");
            }
            else if (recordName == "VSYS" &&
                     (keyword == "TM" || keyword == "SE"))
            {
                edit.expandLocationCode("mts");
            }
        }

        return;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

ListOfPaths
    Manager::getFRUsByUnexpandedLocationCode(const LocationCode locationCode,
                                             const NodeNumber nodeNumber)
{
    ReaderImpl read;
    return read.getFrusAtLocation(locationCode, nodeNumber, fruLocationCode);
}

ListOfPaths
    Manager::getFRUsByExpandedLocationCode(const LocationCode locationCode)
{
    ReaderImpl read;
    return read.getFRUsByExpandedLocationCode(locationCode, fruLocationCode);
}

LocationCode Manager::getExpandedLocationCode(const LocationCode locationCode,
                                              const NodeNumber nodeNumber)
{
    ReaderImpl read;
    return read.getExpandedLocationCode(locationCode, nodeNumber,
                                        fruLocationCode);
}

void Manager::performVPDRecollection()
{
    // get list of FRUs replaceable at standby
    for (const auto& item : replaceableFrus)
    {
        const vector<nlohmann::json>& groupEEPROM = jsonFile["frus"][item];
        const nlohmann::json& singleFru = groupEEPROM[0];

        const string& inventoryPath =
            singleFru["inventoryPath"]
                .get_ref<const nlohmann::json::string_t&>();

        bool prePostActionRequired = false;

        if ((jsonFile["frus"][item].at(0)).find("preAction") !=
            jsonFile["frus"][item].at(0).end())
        {
            if (!executePreAction(jsonFile, item))
            {
                // if the FRU has preAction defined then its execution should
                // pass to ensure bind/unbind of data.
                // preAction execution failed. should not call bind/unbind.
                log<level::ERR>("Pre-Action execution failed for the FRU");
                continue;
            }
            prePostActionRequired = true;
        }

        // unbind, bind the driver to trigger parser.
        triggerVpdCollection(singleFru, inventoryPath);

        // this check is added to avoid file system expensive call in case not
        // required.
        if (prePostActionRequired)
        {
            // Check if device showed up (test for file)
            if (!filesystem::exists(item))
            {
                // If not, then take failure postAction
                executePostFailAction(jsonFile, item);
            }
            else
            {
                // bind the LED driver
                string chipAddr = singleFru.value("pcaChipAddress", "");
                cout << "performVPDRecollection: Executing driver binding for "
                        "chip "
                        "address - "
                     << chipAddr << endl;

                executeCmd(createBindUnbindDriverCmnd(chipAddr, "i2c",
                                                      "leds-pca955x", "/bind"));
            }
        }
    }
}

void Manager::collectFRUVPD(const sdbusplus::message::object_path path)
{
    using InvalidArgument =
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
    using Argument = xyz::openbmc_project::Common::InvalidArgument;

    // if path not found in Json.
    if (frus.find(path) == frus.end())
    {
        elog<InvalidArgument>(
            Argument::ARGUMENT_NAME("Object Path"),
            Argument::ARGUMENT_VALUE(std::string(path).c_str()));
    }

    inventory::Path vpdFilePath = std::get<0>(frus.find(path)->second);

    const std::vector<nlohmann::json>& groupEEPROM =
        jsonFile["frus"][vpdFilePath].get_ref<const nlohmann::json::array_t&>();

    const nlohmann::json& singleFru = groupEEPROM[0];

    // check if the device qualifies for CM.
    if (singleFru.value("concurrentlyMaintainable", false))
    {
        bool prePostActionRequired = false;

        if ((jsonFile["frus"][vpdFilePath].at(0)).find("preAction") !=
            jsonFile["frus"][vpdFilePath].at(0).end())
        {
            if (!executePreAction(jsonFile, vpdFilePath))
            {
                // if the FRU has preAction defined then its execution should
                // pass to ensure bind/unbind of data.
                // preAction execution failed. should not call bind/unbind.
                log<level::ERR>("Pre-Action execution failed for the FRU");
                return;
            }

            prePostActionRequired = true;
        }

        // unbind, bind the driver to trigger parser.
        triggerVpdCollection(singleFru, std::string(path));

        // this check is added to avoid file system expensive call in case not
        // required.
        if (prePostActionRequired)
        {
            // Check if device showed up (test for file)
            if (!filesystem::exists(vpdFilePath))
            {
                // If not, then take failure postAction
                executePostFailAction(jsonFile, vpdFilePath);
            }
            else
            {
                // bind the LED driver
                string chipAddr = jsonFile["frus"][vpdFilePath].at(0).value(
                    "pcaChipAddress", "");
                cout << "Executing driver binding for chip address - "
                     << chipAddr << endl;

                executeCmd(createBindUnbindDriverCmnd(chipAddr, "i2c",
                                                      "leds-pca955x", "/bind"));
            }
        }
        return;
    }
    else
    {
        elog<InvalidArgument>(
            Argument::ARGUMENT_NAME("Object Path"),
            Argument::ARGUMENT_VALUE(std::string(path).c_str()));
    }
}

void Manager::triggerVpdCollection(const nlohmann::json& singleFru,
                                   const std::string& path)
{
    if ((singleFru.find("devAddress") == singleFru.end()) ||
        (singleFru.find("driverType") == singleFru.end()) ||
        (singleFru.find("busType") == singleFru.end()))
    {
        // The FRUs is marked for collection but missing mandatory
        // fields for collection. Log error and return.
        log<level::ERR>(
            "Collection Failed as mandatory field missing in Json",
            entry("ERROR=%s", ("Recollection failed for " + (path)).c_str()));

        return;
    }

    string deviceAddress = singleFru["devAddress"];
    const string& driverType = singleFru["driverType"];
    const string& busType = singleFru["busType"];

    // devTreeStatus flag is present in json as false to mention
    // that the EEPROM is not mentioned in device tree. If this flag
    // is absent consider the value to be true, i.e EEPROM is
    // mentioned in device tree
    if (!singleFru.value("devTreeStatus", true))
    {
        auto pos = deviceAddress.find('-');
        if (pos != string::npos)
        {
            string busNum = deviceAddress.substr(0, pos);
            deviceAddress = "0x" + deviceAddress.substr(pos + 1, string::npos);

            string deleteDevice = "echo" + deviceAddress + " > /sys/bus/" +
                                  busType + "/devices/" + busType + "-" +
                                  busNum + "/delete_device";
            executeCmd(deleteDevice);

            string addDevice = "echo" + driverType + " " + deviceAddress +
                               " > /sys/bus/" + busType + "/devices/" +
                               busType + "-" + busNum + "/new_device";
            executeCmd(addDevice);
        }
        else
        {
            const string& inventoryPath =
                singleFru["inventoryPath"]
                    .get_ref<const nlohmann::json::string_t&>();

            log<level::ERR>(
                "Wrong format of device address in Json",
                entry("ERROR=%s",
                      ("Recollection failed for " + inventoryPath).c_str()));
        }
    }
    else
    {
        executeCmd(createBindUnbindDriverCmnd(deviceAddress, busType,
                                              driverType, "/unbind"));
        executeCmd(createBindUnbindDriverCmnd(deviceAddress, busType,
                                              driverType, "/bind"));
    }
}

void Manager::deleteFRUVPD(const sdbusplus::message::object_path path)
{
    using InvalidArgument =
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
    using Argument = xyz::openbmc_project::Common::InvalidArgument;

    // if path not found in Json.
    if (frus.find(path) == frus.end())
    {
        elog<InvalidArgument>(
            Argument::ARGUMENT_NAME("Object Path"),
            Argument::ARGUMENT_VALUE(std::string(path).c_str()));
    }

    inventory::Path& vpdFilePath = std::get<0>(frus.find(path)->second);

    string chipAddress =
        jsonFile["frus"][vpdFilePath].at(0).value("pcaChipAddress", "");

    // Unbind the LED driver for this FRU
    cout << "Unbinding device- " << chipAddress << endl;
    executeCmd(createBindUnbindDriverCmnd(chipAddress, "i2c", "leds-pca955x",
                                          "/unbind"));

    // if the FRU is not present then log error
    if (readBusProperty(path, "xyz.openbmc_project.Inventory.Item",
                        "Present") == "false")
    {
        elog<InvalidArgument>(
            Argument::ARGUMENT_NAME("FRU not preset"),
            Argument::ARGUMENT_VALUE(std::string(path).c_str()));
    }
    else
    {
        inventory::ObjectMap objectMap = {
            {path,
             {{"xyz.openbmc_project.Inventory.Item", {{"Present", false}}}}}};

        common::utility::callPIM(move(objectMap));
    }
}

} // namespace manager
} // namespace vpd
} // namespace openpower