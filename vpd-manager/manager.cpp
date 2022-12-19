#include "config.h"

#include "manager.hpp"

#include "common_utility.hpp"
#include "editor_impl.hpp"
#include "ibm_vpd_utils.hpp"
#include "ipz_parser.hpp"
#include "parser_factory.hpp"
#include "reader_impl.hpp"
#include "vpd_exceptions.hpp"

#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace openpower::vpd::constants;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::manager::editor;
using namespace openpower::vpd::manager::reader;
using namespace std;
using namespace openpower::vpd::parser;
using namespace openpower::vpd::parser::factory;
using namespace openpower::vpd::ipz::parser;
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
                 std::shared_ptr<sdbusplus::asio::connection>& conn) :
    ioContext(ioCon),
    interface(iFace), conn(conn)
{
    interface->register_method(
        "WriteKeyword",
        [this](const sdbusplus::message::object_path& path,
               const std::string& recordName, const std::string& keyword,
               const Binary& value) {
            this->writeKeyword(path, recordName, keyword, value);
        });

    interface->register_method(
        "GetFRUsByUnexpandedLocationCode",
        [this](const std::string& locationCode,
               const uint16_t nodeNumber) -> inventory::ListOfPaths {
            return this->getFRUsByUnexpandedLocationCode(locationCode,
                                                         nodeNumber);
        });

    interface->register_method(
        "GetFRUsByExpandedLocationCode",
        [this](const std::string& locationCode) -> inventory::ListOfPaths {
            return this->getFRUsByExpandedLocationCode(locationCode);
        });

    interface->register_method(
        "GetExpandedLocationCode",
        [this](const std::string& locationCode,
               const uint16_t nodeNumber) -> std::string {
            return this->getExpandedLocationCode(locationCode, nodeNumber);
        });

    interface->register_method("PerformVPDRecollection",
                               [this]() { this->performVPDRecollection(); });

    interface->register_method(
        "deleteFRUVPD", [this](const sdbusplus::message::object_path& path) {
            this->deleteFRUVPD(path);
        });

    interface->register_method(
        "CollectFRUVPD", [this](const sdbusplus::message::object_path& path) {
            this->collectFRUVPD(path);
        });

    sd_bus_default(&sdBus);
    initManager();
}

void Manager::initManager()
{
    try
    {
        processJSON();
        restoreSystemVpd();
        listenHostState();
        listenAssetTag();

        // Create an instance of the BIOS handler
        biosHandler = std::make_shared<BiosHandler>(conn, *this);

        // instantiate gpioMonitor class
        gpioMon = std::make_shared<GpioMonitor>(jsonFile, ioContext);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
}

/**
 * @brief An api to get list of blank system VPD properties.
 * @param[in] vpdMap - IPZ vpd map.
 * @param[in] objectPath - Object path for the FRU.
 * @param[out] blankPropertyList - Properties which are blank in System VPD and
 * needs to be updated as standby.
 */
static void
    getListOfBlankSystemVpd(Parsed& vpdMap, const string& objectPath,
                            std::vector<RestoredEeproms>& blankPropertyList)
{
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
                        if (kwdValue.find_first_not_of(' ') == string::npos)
                        {
                            // implies data is blank on EEPROM but not on cache.
                            // So EEPROM vpd update is required.
                            Binary busData(busValue.begin(), busValue.end());

                            blankPropertyList.push_back(std::make_tuple(
                                objectPath, recordName, keyword, busData));
                        }
                    }
                }
            }
        }
    }
}

void Manager::restoreSystemVpd()
{
    std::cout << "Attempting system VPD restore" << std::endl;
    ParserInterface* parser = nullptr;
    try
    {
        auto vpdVector = getVpdDataInVector(jsonFile, systemVpdFilePath);
        const auto& inventoryPath =
            jsonFile["frus"][systemVpdFilePath][0]["inventoryPath"]
                .get_ref<const nlohmann::json::string_t&>();

        parser = ParserFactory::getParser(vpdVector, (pimPath + inventoryPath));
        auto parseResult = parser->parse();

        if (auto pVal = std::get_if<Store>(&parseResult))
        {
            // map to hold all the keywords whose value is blank and
            // needs to be updated at standby.
            std::vector<RestoredEeproms> blankSystemVpdProperties{};
            getListOfBlankSystemVpd(pVal->getVpdMap(), SYSTEM_OBJECT,
                                    blankSystemVpdProperties);

            // if system VPD restore is required, update the
            // EEPROM
            for (const auto& item : blankSystemVpdProperties)
            {
                std::cout << "Restoring keyword: " << std::get<2>(item)
                          << std::endl;
                writeKeyword(std::get<0>(item), std::get<1>(item),
                             std::get<2>(item), std::get<3>(item));
            }
        }
        else
        {
            std::cerr << "Not a valid format to restore system VPD"
                      << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to restore system VPD due to exception: "
                  << e.what() << std::endl;
    }
    // release the parser object
    ParserFactory::freeParser(parser);
}

void Manager::listenHostState()
{
    static std::shared_ptr<sdbusplus::bus::match_t> hostState =
        std::make_shared<sdbusplus::bus::match_t>(
            *conn,
            sdbusplus::bus::match::rules::propertiesChanged(
                "/xyz/openbmc_project/state/host0",
                "xyz.openbmc_project.State.Host"),
            [this](sdbusplus::message_t& msg) { hostStateCallBack(msg); });
}

void Manager::checkEssentialFrus()
{
    for (const auto& invPath : essentialFrus)
    {
        const auto res = readBusProperty(invPath, invItemIntf, "Present");

        // implies the essential FRU is missing. Log PEL.
        if (res == "false")
        {
            auto rc = sd_bus_call_method_async(
                sdBus, NULL, loggerService, loggerObjectPath,
                loggerCreateInterface, "Create", NULL, NULL, "ssa{ss}",
                errIntfForEssentialFru,
                "xyz.openbmc_project.Logging.Entry.Level.Warning", 2,
                "DESCRIPTION", "Essential fru missing from the system.",
                "CALLOUT_INVENTORY_PATH", (pimPath + invPath).c_str());

            if (rc < 0)
            {
                log<level::ERR>("Error calling sd_bus_call_method_async",
                                entry("RC=%d", rc),
                                entry("MSG=%s", strerror(-rc)));
            }
        }
    }
}

void Manager::hostStateCallBack(sdbusplus::message_t& msg)
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
                // detect if essential frus are present in the system.
                checkEssentialFrus();

                // check and perfrom recollection for FRUs replaceable at
                // standby.
                performVPDRecollection();
                return;
            }
        }
    }
}

void Manager::listenAssetTag()
{
    static std::shared_ptr<sdbusplus::bus::match_t> assetMatcher =
        std::make_shared<sdbusplus::bus::match_t>(
            *conn,
            sdbusplus::bus::match::rules::propertiesChanged(
                "/xyz/openbmc_project/inventory/system",
                "xyz.openbmc_project.Inventory.Decorator.AssetTag"),
            [this](sdbusplus::message_t& msg) { assetTagCallback(msg); });
}

void Manager::assetTagCallback(sdbusplus::message_t& msg)
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

            if (itemEEPROM.value("essentialFru", false))
            {
                essentialFrus.emplace_back(itemEEPROM["inventoryPath"]);
            }
        }
    }
}

void Manager::writeKeyword(const sdbusplus::message::object_path& path,
                           const std::string& recordName,
                           const std::string& keyword, const Binary& value)
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
                            recordName, keyword, objPath);
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
    Manager::getFRUsByUnexpandedLocationCode(const LocationCode& locationCode,
                                             const NodeNumber nodeNumber)
{
    ReaderImpl read;
    return read.getFrusAtLocation(locationCode, nodeNumber, fruLocationCode);
}

ListOfPaths
    Manager::getFRUsByExpandedLocationCode(const LocationCode& locationCode)
{
    ReaderImpl read;
    return read.getFRUsByExpandedLocationCode(locationCode, fruLocationCode);
}

LocationCode Manager::getExpandedLocationCode(const LocationCode& locationCode,
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
            try
            {
                if (!executePreAction(jsonFile, item))
                {
                    // if the FRU has preAction defined then its execution
                    // should pass to ensure bind/unbind of data.
                    // preAction execution failed. should not call
                    // bind/unbind.
                    log<level::ERR>(
                        "Pre-Action execution failed for the FRU",
                        entry("ERROR=%s",
                              ("Inventory path: " + inventoryPath).c_str()));
                    continue;
                }
            }
            catch (const GpioException& e)
            {
                log<level::ERR>(e.what());
                PelAdditionalData additionalData{};
                additionalData.emplace("DESCRIPTION", e.what());
                createPEL(additionalData, PelSeverity::WARNING,
                          errIntfForGpioError, sdBus);
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
                try
                {
                    // If not, then take failure postAction
                    executePostFailAction(jsonFile, item);
                }
                catch (const GpioException& e)
                {
                    PelAdditionalData additionalData{};
                    additionalData.emplace("DESCRIPTION", e.what());
                    createPEL(additionalData, PelSeverity::WARNING,
                              errIntfForGpioError, sdBus);
                }
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

void Manager::collectFRUVPD(const sdbusplus::message::object_path& path)
{
    std::cout << "Manager called to collect vpd for fru: " << std::string{path}
              << std::endl;

    using InvalidArgument =
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
    using Argument = xyz::openbmc_project::Common::InvalidArgument;

    std::string objPath{path};

    // Strip any inventory prefix in path
    if (objPath.find(INVENTORY_PATH) == 0)
    {
        objPath = objPath.substr(sizeof(INVENTORY_PATH) - 1);
    }

    // if path not found in Json.
    if (frus.find(objPath) == frus.end())
    {
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("Object Path"),
                              Argument::ARGUMENT_VALUE(objPath.c_str()));
    }

    inventory::Path vpdFilePath = std::get<0>(frus.find(objPath)->second);

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
        triggerVpdCollection(singleFru, objPath);

        // this check is added to avoid file system expensive call in case not
        // required.
        if (prePostActionRequired)
        {
            // Check if device showed up (test for file)
            if (!filesystem::exists(vpdFilePath))
            {
                try
                {
                    // If not, then take failure postAction
                    executePostFailAction(jsonFile, vpdFilePath);
                }
                catch (const GpioException& e)
                {
                    PelAdditionalData additionalData{};
                    additionalData.emplace("DESCRIPTION", e.what());
                    createPEL(additionalData, PelSeverity::WARNING,
                              errIntfForGpioError, sdBus);
                }
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
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("Object Path"),
                              Argument::ARGUMENT_VALUE(objPath.c_str()));
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

void Manager::deleteFRUVPD(const sdbusplus::message::object_path& path)
{
    std::cout << "Manager called to delete vpd for fru: " << std::string{path}
              << std::endl;

    using InvalidArgument =
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
    using Argument = xyz::openbmc_project::Common::InvalidArgument;

    std::string objPath{path};

    // Strip any inventory prefix in path
    if (objPath.find(INVENTORY_PATH) == 0)
    {
        objPath = objPath.substr(sizeof(INVENTORY_PATH) - 1);
    }

    // if path not found in Json.
    if (frus.find(objPath) == frus.end())
    {
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("Object Path"),
                              Argument::ARGUMENT_VALUE(objPath.c_str()));
    }

    inventory::Path& vpdFilePath = std::get<0>(frus.find(objPath)->second);

    string chipAddress =
        jsonFile["frus"][vpdFilePath].at(0).value("pcaChipAddress", "");

    // Unbind the LED driver for this FRU
    cout << "Unbinding device- " << chipAddress << endl;
    executeCmd(createBindUnbindDriverCmnd(chipAddress, "i2c", "leds-pca955x",
                                          "/unbind"));

    // if the FRU is not present then log error
    if (readBusProperty(objPath, "xyz.openbmc_project.Inventory.Item",
                        "Present") == "false")
    {
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("FRU not preset"),
                              Argument::ARGUMENT_VALUE(objPath.c_str()));
    }
    else
    {
        // Set present property of FRU as false as it has been removed.
        // CC data for FRU is also removed as
        // a) FRU is not there so CC does not make sense.
        // b) Sensors dependent on Panel uses CC data.
        inventory::InterfaceMap interfaces{
            {"xyz.openbmc_project.Inventory.Item", {{"Present", false}}},
            {"com.ibm.ipzvpd.VINI", {{"CC", Binary{}}}}};

        inventory::ObjectMap objectMap;
        objectMap.emplace(objPath, move(interfaces));

        common::utility::callPIM(move(objectMap));
    }
}

} // namespace manager
} // namespace vpd
} // namespace openpower