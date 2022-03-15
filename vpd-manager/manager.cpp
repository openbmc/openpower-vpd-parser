#include "config.h"

#include "manager.hpp"

#include "editor_impl.hpp"
#include "error.hpp"
#include "ibm_vpd_utils.hpp"
#include "ipz_parser.hpp"
#include "reader_impl.hpp"
#include "vpd_exceptions.hpp"

#include <phosphor-logging/elog-errors.hpp>

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
Manager::Manager(std::shared_ptr<boost::asio::io_context>& ioCon,
                 std::shared_ptr<sdbusplus::asio::dbus_interface>& iFace,
                 std::shared_ptr<sdbusplus::asio::connection>& connection) :
    ioContext(ioCon),
    interface(iFace), conn(connection)
{
    interface->register_method(
        "writeKeyword", [this](const sdbusplus::message::object_path& path,
                               const std::string& recordName,
                               const std::string& keyword, const Binary value) {
            this->writeKeyword(path, recordName, keyword, value);
        });

    interface->register_method(
        "getFRUsByUnexpandedLocationCode",
        [this](const std::string& locationCode,
               const uint16_t nodeNumber) -> inventory::ListOfPaths {
            return this->getFRUsByUnexpandedLocationCode(locationCode,
                                                         nodeNumber);
        });

    interface->register_method(
        "getFRUsByExpandedLocationCode",
        [this](const std::string& locationCode) -> inventory::ListOfPaths {
            return this->getFRUsByExpandedLocationCode(locationCode);
        });

    interface->register_method(
        "getExpandedLocationCode",
        [this](const std::string& locationCode,
               const uint16_t nodeNumber) -> std::string {
            return this->getExpandedLocationCode(locationCode, nodeNumber);
        });

    interface->register_method(
        "performVPDRecollection",
        [this](const std::string& locationCode, const uint16_t nodeNumber) {
            this->getExpandedLocationCode(locationCode, nodeNumber);
        });

    initManager();
}

void Manager::initManager()
{
    try
    {
        processJSON();
        listenHostState();

        // instantiate gpioMonitor class
        gpioMon = std::make_shared<GpioMonitor>(jsonFile, ioContext);
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
            *conn,
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
        // just as an example to show how it will be used for functions exposed
        // on Dbus.
        // throw DbusFailure();

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

        if ((singleFru.find("devAddress") == singleFru.end()) ||
            (singleFru.find("driverType") == singleFru.end()) ||
            (singleFru.find("busType") == singleFru.end()))
        {
            // The FRUs is marked for replacement but missing mandatory
            // fields for recollection. Skip to another replaceable fru.
            log<level::ERR>(
                "Recollection Failed as mandatory field missing in Json",
                entry("ERROR=%s",
                      ("Recollection failed for " + inventoryPath).c_str()));
            continue;
        }

        string str = "echo ";
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
                deviceAddress =
                    "0x" + deviceAddress.substr(pos + 1, string::npos);

                string deleteDevice = str + deviceAddress + " > /sys/bus/" +
                                      busType + "/devices/" + busType + "-" +
                                      busNum + "/delete_device";
                executeCmd(deleteDevice);

                string addDevice = str + driverType + " " + deviceAddress +
                                   " > /sys/bus/" + busType + "/devices/" +
                                   busType + "-" + busNum + "/new_device";
                executeCmd(addDevice);
            }
            else
            {
                log<level::ERR>(
                    "Wrong format of device address in Json",
                    entry(
                        "ERROR=%s",
                        ("Recollection failed for " + inventoryPath).c_str()));
                continue;
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
}

} // namespace manager
} // namespace vpd
} // namespace openpower