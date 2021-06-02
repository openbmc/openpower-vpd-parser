#include "config.h"

#include "manager.hpp"

#include "editor_impl.hpp"
#include "ibm_vpd_utils.hpp"
#include "ipz_parser.hpp"
#include "reader_impl.hpp"
#include "vpd_exceptions.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace openpower::vpd::constants;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::manager::editor;
using namespace openpower::vpd::manager::reader;
using namespace std;
using namespace openpower::vpd::parser;
using namespace openpower::vpd::exceptions;
using namespace phosphor::logging;
using json = nlohmann::json;

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

void Manager::getLocationCodeToInvMap(
    inventory::LocationCodeMap& fruLocationCode, const json& jsonFile)
{
    if (jsonFile.find("frus") == jsonFile.end())
    {
        throw VpdJsonException("frus group not found in Inventory json",
                               INVENTORY_JSON_SYM_LINK);
    }
    const json& groupFRUS = jsonFile["frus"].get_ref<const json::object_t&>();
    for (const auto& itemFRUS : groupFRUS.items())
    {
        const std::vector<json>& groupEEPROM =
            itemFRUS.value().get_ref<const json::array_t&>();
        for (const auto& itemEEPROM : groupEEPROM)
        {
            bool isMotherboard = false;
            if (itemEEPROM["extraInterfaces"].find(
                    "xyz.openbmc_project.Inventory.Item.Board.Motherboard") !=
                itemEEPROM["extraInterfaces"].end())
            {
                isMotherboard = true;
            }
            frus.emplace(itemEEPROM["inventoryPath"]
                             .get_ref<const nlohmann::json::string_t&>(),
                         std::make_pair(itemFRUS.key(), isMotherboard));

            if (itemEEPROM["extraInterfaces"].find(IBM_LOCATION_CODE_INF) !=
                itemEEPROM["extraInterfaces"].end())
            {
                fruLocationCode.emplace(
                    itemEEPROM["extraInterfaces"][IBM_LOCATION_CODE_INF]
                              ["LocationCode"]
                                  .get_ref<const json::string_t&>(),
                    itemEEPROM["inventoryPath"]
                        .get_ref<const json::string_t&>());
            }
        }
    }
}

void Manager::getReplaceableFruVector(ReplaceableFrus& replaceableFrus,
                                      const json jsonFile)
{
    const json& groupFRUS = jsonFile["frus"].get_ref<const json::object_t&>();
    for (const auto& itemFRUS : groupFRUS.items())
    {
        const std::vector<json>& groupEEPROM =
            itemFRUS.value().get_ref<const json::array_t&>();
        for (const auto& itemEEPROM : groupEEPROM)
        {
            if (itemEEPROM.value("isReplaceable", false))
            {
                replaceableFrus.emplace_back(itemFRUS.key());
            }
        }
    }
}

void Manager::run()
{
    try
    {
        processJSON();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    while (true)
    {
        _bus.process_discard();

        // wait for event
        _bus.wait();
    }
}

void Manager::processJSON()
{
    getParsedInventoryJsonObject(jsonFile);
    getInvToEepromMap(frus, jsonFile);
    getLocationCodeToInvMap(fruLocationCode, jsonFile);
    getReplaceableFruVector(replaceableFrus, jsonFile);
}

void Manager::writeKeyword(const sdbusplus::message::object_path path,
                           const std::string recordName,
                           const std::string keyword, const Binary value)
{
    try
    {
        if (frus.find(path) == frus.end())
        {
            throw std::runtime_error("Inventory path not found");
        }

        inventory::Path vpdFilePath = frus.find(path)->second.first;

        // instantiate editor class to update the data
        EditorImpl edit(vpdFilePath, jsonFile, recordName, keyword, path);
        edit.updateKeyword(value, true);

        // if it is a motehrboard FRU need to check for location expansion
        if (frus.find(path)->second.second)
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

        if ((singleFru.find("bind") == singleFru.end()) ||
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
        triggerVpdCollection(singleFru);
    }
}

void Manager::collectSingleFruVpd(const sdbusplus::message::object_path path)
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

    inventory::Path vpdFilePath = frus.find(path)->second.first;

    const std::vector<json>& groupEEPROM =
        jsonFile["frus"][vpdFilePath].get_ref<const json::array_t&>();

    const nlohmann::json& singleFru = groupEEPROM[0];

    // check if the device qualifies for CM.
    if (singleFru.value("concurrentlyMaintainable", false))
    {
        if ((singleFru.find("bind") == singleFru.end()) ||
            (singleFru.find("driverType") == singleFru.end()) ||
            (singleFru.find("busType") == singleFru.end()))
        {
            // The FRUs is marked for collection but missing mandatory
            // fields for collection. Log error and return.
            log<level::ERR>(
                "Collection Failed as mandatory field missing in Json",
                entry(
                    "ERROR=%s",
                    ("Recollection failed for " + std::string(path)).c_str()));

            return;
        }

        // unbind, bind the driver to trigger parser.
        triggerVpdCollection(singleFru);
        return;
    }
    else
    {
        elog<InvalidArgument>(
            Argument::ARGUMENT_NAME("Object Path"),
            Argument::ARGUMENT_VALUE(std::string(path).c_str()));
    }
}

void Manager::triggerVpdCollection(const nlohmann::json& singleFru)
{
    string str = "echo ";
    string deviceAddress = singleFru["bind"];
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
        string cmd = str + deviceAddress + " > /sys/bus/" + busType +
                     "/drivers/" + driverType;

        executeCmd(cmd + "/unbind");
        executeCmd(cmd + "/bind");
    }
}

} // namespace manager
} // namespace vpd
} // namespace openpower
