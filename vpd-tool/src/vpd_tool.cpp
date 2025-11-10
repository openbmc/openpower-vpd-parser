#include "config.h"

#include "vpd_tool.hpp"

#include "tool_constants.hpp"
#include "tool_types.hpp"
#include "tool_utils.hpp"

#include <cstdlib>
#include <iostream>
#include <regex>
#include <tuple>
namespace vpd
{
// {Record, Keyword} -> {attribute name, number of bits in keyword, starting bit
// position, enabled value, disabled value}
// Note: we do not care about min/max value for the BIOS attribute here.
const types::BiosAttributeKeywordMap VpdTool::m_biosAttributeVpdKeywordMap = {
    {{"VSYS", "MM"},
     {{"hb_memory_mirror_mode", constants::VALUE_8, std::nullopt,
       constants::VALUE_2, constants::VALUE_1}}},
    {{"VSYS", "BA"},
     {{"pvm_keep_and_clear", constants::VALUE_1, constants::VALUE_0,
       constants::VALUE_1, constants::VALUE_0},
      {"pvm_create_default_lpar", constants::VALUE_1, constants::VALUE_1,
       constants::VALUE_1, constants::VALUE_0},
      {"pvm_clear_nvram", constants::VALUE_1, constants::VALUE_2,
       constants::VALUE_1, constants::VALUE_0}}},
    {{"VSYS", "RG"},
     {{"hb_field_core_override", constants::VALUE_32, std::nullopt,
       std::nullopt, std::nullopt}}}};

int VpdTool::readKeyword(
    const std::string& i_vpdPath, const std::string& i_recordName,
    const std::string& i_keywordName, const bool i_onHardware,
    const std::string& i_fileToSave)
{
    int l_rc = constants::FAILURE;
    try
    {
        types::DbusVariantType l_keywordValue;
        if (i_onHardware)
        {
            l_keywordValue = utils::readKeywordFromHardware(
                i_vpdPath, std::make_tuple(i_recordName, i_keywordName));
        }
        else
        {
            std::string l_inventoryObjectPath(
                constants::baseInventoryPath + i_vpdPath);

            l_keywordValue = utils::readDbusProperty(
                constants::inventoryManagerService, l_inventoryObjectPath,
                constants::ipzVpdInfPrefix + i_recordName, i_keywordName);
        }

        if (const auto l_value =
                std::get_if<types::BinaryVector>(&l_keywordValue);
            l_value && !l_value->empty())
        {
            // ToDo: Print value in both ASCII and hex formats
            const std::string& l_keywordStrValue =
                utils::getPrintableValue(*l_value);

            if (i_fileToSave.empty())
            {
                utils::displayOnConsole(i_vpdPath, i_keywordName,
                                        l_keywordStrValue);
                l_rc = constants::SUCCESS;
            }
            else
            {
                if (utils::saveToFile(i_fileToSave, l_keywordStrValue))
                {
                    std::cout
                        << "Value read is saved on the file: " << i_fileToSave
                        << std::endl;
                    l_rc = constants::SUCCESS;
                }
                else
                {
                    std::cerr
                        << "Error while saving the read value on the file: "
                        << i_fileToSave
                        << "\nDisplaying the read value on console"
                        << std::endl;
                    utils::displayOnConsole(i_vpdPath, i_keywordName,
                                            l_keywordStrValue);
                }
            }
        }
        else
        {
            // TODO: Enable logging when verbose is enabled.
            std::cout << "Invalid data type or empty data received."
                      << std::endl;
        }
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << "Read keyword's value failed for path: " << i_vpdPath
                  << ", Record: " << i_recordName << ", Keyword: "
                  << i_keywordName << ", error: " << l_ex.what() << std::endl;
    }
    return l_rc;
}

int VpdTool::dumpObject(std::string i_fruPath) const noexcept
{
    int l_rc{constants::FAILURE};
    try
    {
        // ToDo: For PFuture system take only full path from the user.
        i_fruPath = constants::baseInventoryPath + i_fruPath;

        nlohmann::json l_resultJsonArray = nlohmann::json::array({});
        const nlohmann::json l_fruJson = getFruProperties(i_fruPath);
        if (!l_fruJson.empty())
        {
            l_resultJsonArray += l_fruJson;

            utils::printJson(l_resultJsonArray);
        }
        else
        {
            std::cout << "FRU [" << i_fruPath
                      << "] is not present in the system" << std::endl;
        }
        l_rc = constants::SUCCESS;
    }
    catch (std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << "Dump Object failed for FRU [" << i_fruPath
                  << "], Error: " << l_ex.what() << std::endl;
    }
    return l_rc;
}

template <typename PropertyType>
void VpdTool::populateInterfaceJson(const std::string& i_inventoryObjPath,
                                    const std::string& i_infName,
                                    const std::vector<std::string>& i_propList,
                                    nlohmann::json& io_fruJsonObject) const
{
    nlohmann::json l_interfaceJsonObj = nlohmann::json::object({});

    auto l_readProperties = [i_inventoryObjPath, &l_interfaceJsonObj, i_infName,
                             this](const std::string& i_property) {
        const nlohmann::json l_propertyJsonObj =
            getInventoryPropertyJson<PropertyType>(i_inventoryObjPath,
                                                   i_infName, i_property);
        l_interfaceJsonObj.insert(l_propertyJsonObj.cbegin(),
                                  l_propertyJsonObj.cend());
    };

    std::for_each(i_propList.cbegin(), i_propList.cend(), l_readProperties);

    if (!l_interfaceJsonObj.empty())
    {
        io_fruJsonObject.insert(l_interfaceJsonObj.cbegin(),
                                l_interfaceJsonObj.cend());
    }
}

void VpdTool::populateFruJson(
    const std::string& i_inventoryObjPath, nlohmann::json& io_fruJsonObject,
    const std::vector<std::string>& i_interfaceList) const
{
    for (const auto& l_interface : i_interfaceList)
    {
        if (l_interface == constants::inventoryItemInf)
        {
            const std::vector<std::string> l_properties = {"PrettyName"};
            populateInterfaceJson<std::string>(i_inventoryObjPath,
                                               constants::inventoryItemInf,
                                               l_properties, io_fruJsonObject);
            continue;
        }

        if (l_interface == constants::xyzLocationCodeInf)
        {
            const std::vector<std::string> l_properties = {"LocationCode"};
            populateInterfaceJson<std::string>(i_inventoryObjPath,
                                               constants::xyzLocationCodeInf,
                                               l_properties, io_fruJsonObject);
            continue;
        }

        if (l_interface == constants::viniInf)
        {
            const std::vector<std::string> l_properties = {"SN", "PN", "CC",
                                                           "FN", "DR"};
            populateInterfaceJson<vpd::types::BinaryVector>(
                i_inventoryObjPath, constants::viniInf, l_properties,
                io_fruJsonObject);
            continue;
        }

        if (l_interface == constants::assetInf)
        {
            if (std::find(i_interfaceList.begin(), i_interfaceList.end(),
                          constants::viniInf) != i_interfaceList.end())
            {
                // The value will be filled from VINI interface. Don't
                // process asset interface.
                continue;
            }

            const std::vector<std::string> l_properties = {
                "Model", "SerialNumber", "SubModel"};

            populateInterfaceJson<std::string>(i_inventoryObjPath,
                                               constants::assetInf,
                                               l_properties, io_fruJsonObject);
            continue;
        }

        if (l_interface == constants::networkInf)
        {
            const std::vector<std::string> l_properties = {"MACAddress"};
            populateInterfaceJson<std::string>(i_inventoryObjPath,
                                               constants::networkInf,
                                               l_properties, io_fruJsonObject);
            continue;
        }

        if (l_interface == constants::pcieSlotInf)
        {
            const std::vector<std::string> l_properties = {"SlotType"};
            populateInterfaceJson<std::string>(i_inventoryObjPath,
                                               constants::pcieSlotInf,
                                               l_properties, io_fruJsonObject);
            continue;
        }

        if (l_interface == constants::slotNumInf)
        {
            const std::vector<std::string> l_properties = {"SlotNumber"};
            populateInterfaceJson<uint32_t>(i_inventoryObjPath,
                                            constants::slotNumInf, l_properties,
                                            io_fruJsonObject);
            continue;
        }

        if (l_interface == constants::i2cDeviceInf)
        {
            const std::vector<std::string> l_properties = {"Address", "Bus"};
            populateInterfaceJson<uint32_t>(i_inventoryObjPath,
                                            constants::i2cDeviceInf,
                                            l_properties, io_fruJsonObject);
            continue;
        }
    }
}

nlohmann::json VpdTool::getFruProperties(const std::string& i_objectPath) const
{
    // check if FRU is present in the system
    if (!isFruPresent(i_objectPath))
    {
        return nlohmann::json::object_t();
    }

    nlohmann::json l_fruJson = nlohmann::json::object_t({});

    // need to trim out the base inventory path in the FRU JSON.
    const std::string l_displayObjectPath =
        (i_objectPath.find(constants::baseInventoryPath) == std::string::npos)
            ? i_objectPath
            : i_objectPath.substr(strlen(constants::baseInventoryPath));

    l_fruJson.emplace(l_displayObjectPath, nlohmann::json::object_t({}));

    auto& l_fruObject = l_fruJson[l_displayObjectPath];

    types::MapperGetObject l_mapperResp = utils::GetServiceInterfacesForObject(
        i_objectPath, std::vector<std::string>{});

    for (const auto& [l_service, l_interfaceList] : l_mapperResp)
    {
        if (l_service != constants::inventoryManagerService)
        {
            continue;
        }
        populateFruJson(i_objectPath, l_fruObject, l_interfaceList);
    }

    const auto l_typePropertyJson = getFruTypeProperty(i_objectPath);
    if (!l_typePropertyJson.empty())
    {
        l_fruObject.insert(l_typePropertyJson.cbegin(),
                           l_typePropertyJson.cend());
    }

    // insert FRU "TYPE"
    l_fruObject.emplace("TYPE", "FRU");

    return l_fruJson;
}

template <typename PropertyType>
nlohmann::json VpdTool::getInventoryPropertyJson(
    const std::string& i_objectPath, const std::string& i_interface,
    const std::string& i_propertyName) const noexcept
{
    nlohmann::json l_resultInJson = nlohmann::json::object({});
    try
    {
        types::DbusVariantType l_keyWordValue;

        l_keyWordValue =
            utils::readDbusProperty(constants::inventoryManagerService,
                                    i_objectPath, i_interface, i_propertyName);

        if (const auto l_value = std::get_if<PropertyType>(&l_keyWordValue))
        {
            if constexpr (std::is_same<PropertyType, std::string>::value)
            {
                l_resultInJson.emplace(i_propertyName, *l_value);
            }
            else if constexpr (std::is_same<PropertyType, bool>::value)
            {
                l_resultInJson.emplace(i_propertyName,
                                       *l_value ? "true" : "false");
            }
            else if constexpr (std::is_same<PropertyType,
                                            types::BinaryVector>::value)
            {
                const std::string& l_keywordStrValue =
                    vpd::utils::getPrintableValue(*l_value);

                l_resultInJson.emplace(i_propertyName, l_keywordStrValue);
            }
            else if constexpr (std::is_same<PropertyType, uint32_t>::value)
            {
                l_resultInJson.emplace(i_propertyName,
                                       std::to_string(*l_value));
            }
        }
        else
        {
            // TODO: Enable logging when verbose is enabled.
            std::cout << "Invalid data type received." << std::endl;
        }
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << "Read " << i_propertyName
                  << " value for FRU path: " << i_objectPath
                  << ", failed with exception: " << l_ex.what() << std::endl;
    }
    return l_resultInJson;
}

int VpdTool::fixSystemVpd() const noexcept
{
    int l_rc = constants::FAILURE;

    nlohmann::json l_backupRestoreCfgJsonObj = getBackupRestoreCfgJsonObj();
    if (!fetchKeywordInfo(l_backupRestoreCfgJsonObj))
    {
        return l_rc;
    }

    printSystemVpd(l_backupRestoreCfgJsonObj);

    do
    {
        printFixSystemVpdOption(types::UserOption::UseBackupDataForAll);
        printFixSystemVpdOption(
            types::UserOption::UseSystemBackplaneDataForAll);
        printFixSystemVpdOption(types::UserOption::MoreOptions);
        printFixSystemVpdOption(types::UserOption::Exit);

        int l_userSelectedOption = types::UserOption::Exit;
        std::cin >> l_userSelectedOption;

        std::cout << std::endl << std::string(191, '=') << std::endl;

        if (types::UserOption::UseBackupDataForAll == l_userSelectedOption)
        {
            l_rc = updateAllKeywords(l_backupRestoreCfgJsonObj, true);
            break;
        }
        else if (types::UserOption::UseSystemBackplaneDataForAll ==
                 l_userSelectedOption)
        {
            l_rc = updateAllKeywords(l_backupRestoreCfgJsonObj, false);
            break;
        }
        else if (types::UserOption::MoreOptions == l_userSelectedOption)
        {
            l_rc = handleMoreOption(l_backupRestoreCfgJsonObj);
            break;
        }
        else if (types::UserOption::Exit == l_userSelectedOption)
        {
            std::cout << "Exit successfully" << std::endl;
            break;
        }
        else
        {
            std::cout << "Provide a valid option. Retry." << std::endl;
        }
    } while (true);

    return l_rc;
}

int VpdTool::writeKeyword(
    std::string i_vpdPath, const std::string& i_recordName,
    const std::string& i_keywordName, const std::string& i_keywordValue,
    const bool i_onHardware) noexcept
{
    int l_rc = constants::FAILURE;
    try
    {
        if (i_vpdPath.empty() || i_recordName.empty() ||
            i_keywordName.empty() || i_keywordValue.empty())
        {
            throw std::runtime_error("Received input is empty.");
        }

        auto l_paramsToWrite =
            std::make_tuple(i_recordName, i_keywordName,
                            utils::convertToBinary(i_keywordValue));

        if (i_onHardware)
        {
            l_rc = utils::writeKeywordOnHardware(i_vpdPath, l_paramsToWrite);
        }
        else
        {
            i_vpdPath = constants::baseInventoryPath + i_vpdPath;
            l_rc = utils::writeKeyword(i_vpdPath, l_paramsToWrite);
        }

        if (l_rc > 0)
        {
            std::cout << "Data updated successfully " << std::endl;
            l_rc = constants::SUCCESS;
        }
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Enable log when verbose is enabled.
        std::cerr << "Write keyword's value for path: " << i_vpdPath
                  << ", Record: " << i_recordName
                  << ", Keyword: " << i_keywordName
                  << " is failed. Exception: " << l_ex.what() << std::endl;
    }
    return l_rc;
}

nlohmann::json VpdTool::getBackupRestoreCfgJsonObj() const noexcept
{
    nlohmann::json l_parsedBackupRestoreJson{};
    try
    {
        nlohmann::json l_parsedSystemJson =
            utils::getParsedJson(INVENTORY_JSON_SYM_LINK);

        // check for mandatory fields at this point itself.
        if (!l_parsedSystemJson.contains("backupRestoreConfigPath"))
        {
            throw std::runtime_error(
                "backupRestoreConfigPath tag is missing from system config JSON : " +
                std::string(INVENTORY_JSON_SYM_LINK));
        }

        l_parsedBackupRestoreJson =
            utils::getParsedJson(l_parsedSystemJson["backupRestoreConfigPath"]);
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << l_ex.what() << std::endl;
    }

    return l_parsedBackupRestoreJson;
}

int VpdTool::cleanSystemVpd(bool i_syncBiosAttributesRequired) const noexcept
{
    try
    {
        // In order to do syncBiosAttributes, we need BIOS Config Manager
        // service up and running
        if (i_syncBiosAttributesRequired &&
            !utils::isServiceRunning(constants::biosConfigMgrService))
        {
            std::cerr
                << "Cannot sync BIOS attributes as BIOS Config Manager service is not running."
                << std::endl;
            return constants::FAILURE;
        }

        // get the keyword map from backup_restore json
        // iterate through the keyword map get default value of
        // l_keywordName.
        // use writeKeyword API to update default value on hardware,
        // backup and D - Bus.
        const nlohmann::json l_parsedBackupRestoreJson =
            getBackupRestoreCfgJsonObj();

        // check for mandatory tags
        if (l_parsedBackupRestoreJson.contains("source") &&
            l_parsedBackupRestoreJson.contains("backupMap") &&
            l_parsedBackupRestoreJson["source"].contains("hardwarePath") &&
            l_parsedBackupRestoreJson["backupMap"].is_array())
        {
            // get the source hardware path
            const auto& l_hardwarePath =
                l_parsedBackupRestoreJson["source"]["hardwarePath"];

            // iterate through the backup map
            for (const auto& l_aRecordKwInfo :
                 l_parsedBackupRestoreJson["backupMap"])
            {
                // check if Manufacturing Reset is required for this entry
                const bool l_isMfgCleanRequired =
                    l_aRecordKwInfo.value("isManufactureResetRequired", false);

                if (l_isMfgCleanRequired)
                {
                    // get the Record name and Keyword name
                    const std::string& l_srcRecordName =
                        l_aRecordKwInfo.value("sourceRecord", "");
                    const std::string& l_srcKeywordName =
                        l_aRecordKwInfo.value("sourceKeyword", "");

                    // validate the Record name, Keyword name and the
                    // defaultValue
                    if (!l_srcRecordName.empty() && !l_srcKeywordName.empty() &&
                        l_aRecordKwInfo.contains("defaultValue") &&
                        l_aRecordKwInfo["defaultValue"].is_array())
                    {
                        // check if this keyword is used for backing up BIOS
                        // attribute
                        const bool l_isUsedForBiosAttributeBackup =
                            l_aRecordKwInfo.value("isBiosSyncRequired", false);

                        const types::BinaryVector l_keywordValueToUpdate =
                            (i_syncBiosAttributesRequired &&
                             l_isUsedForBiosAttributeBackup)
                                ? getVpdValueInBiosConfigManager(
                                      l_srcRecordName, l_srcKeywordName)
                                : l_aRecordKwInfo["defaultValue"]
                                      .get<types::BinaryVector>();

                        if (l_keywordValueToUpdate.empty())
                        {
                            std::cerr << "Failed to update " << l_srcRecordName
                                      << ":" << l_srcKeywordName
                                      << " . Keyword value to update is empty"
                                      << std::endl;
                            continue;
                        }

                        // update the Keyword with default value, use D-Bus
                        // method UpdateKeyword exposed by vpd-manager.
                        // Note: writing to all paths (Primary EEPROM path,
                        // Secondary EEPROM path, D-Bus cache and Backup path)
                        // is the responsibility of vpd-manager's UpdateKeyword
                        // API
                        if (constants::FAILURE ==
                            utils::writeKeyword(
                                l_hardwarePath,
                                std::make_tuple(l_srcRecordName,
                                                l_srcKeywordName,
                                                l_keywordValueToUpdate)))
                        {
                            // TODO: Enable logging when verbose
                            // is enabled.
                            std::cerr << "Failed to update " << l_srcRecordName
                                      << ":" << l_srcKeywordName << std::endl;
                        }
                    }
                    else
                    {
                        std::cerr
                            << "Unrecognized Entry Record [" << l_srcRecordName
                            << "] Keyword [" << l_srcKeywordName
                            << "] in Backup Restore JSON backup map"
                            << std::endl;
                    }
                } // mfgClean required check
            } // keyword list loop
        }
        else // backupRestoreJson is not valid
        {
            std::cerr << "Backup Restore JSON is not valid" << std::endl;
        }

        // success/failure message
        std::cout << "The critical keywords from system backplane VPD has "
                     "been reset successfully."
                  << std::endl;

    } // try block end
    catch (const std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr
            << "Manufacturing reset on system vpd keywords is unsuccessful. Error : "
            << l_ex.what() << std::endl;
    }
    return constants::SUCCESS;
}

bool VpdTool::fetchKeywordInfo(nlohmann::json& io_parsedJsonObj) const noexcept
{
    bool l_returnValue = false;
    try
    {
        if (io_parsedJsonObj.empty() || !io_parsedJsonObj.contains("source") ||
            !io_parsedJsonObj.contains("destination") ||
            !io_parsedJsonObj.contains("backupMap"))
        {
            throw std::runtime_error("Invalid JSON");
        }

        std::string l_srcVpdPath;
        std::string l_dstVpdPath;

        bool l_isSourceOnHardware = false;
        if (l_srcVpdPath = io_parsedJsonObj["source"].value("hardwarePath", "");
            !l_srcVpdPath.empty())
        {
            l_isSourceOnHardware = true;
        }
        else if (l_srcVpdPath =
                     io_parsedJsonObj["source"].value("inventoryPath", "");
                 l_srcVpdPath.empty())
        {
            throw std::runtime_error("Source path is empty in JSON");
        }

        bool l_isDestinationOnHardware = false;
        if (l_dstVpdPath =
                io_parsedJsonObj["destination"].value("hardwarePath", "");
            !l_dstVpdPath.empty())
        {
            l_isDestinationOnHardware = true;
        }
        else if (l_dstVpdPath =
                     io_parsedJsonObj["destination"].value("inventoryPath", "");
                 l_dstVpdPath.empty())
        {
            throw std::runtime_error("Destination path is empty in JSON");
        }

        for (auto& l_aRecordKwInfo : io_parsedJsonObj["backupMap"])
        {
            const std::string& l_srcRecordName =
                l_aRecordKwInfo.value("sourceRecord", "");
            const std::string& l_srcKeywordName =
                l_aRecordKwInfo.value("sourceKeyword", "");
            const std::string& l_dstRecordName =
                l_aRecordKwInfo.value("destinationRecord", "");
            const std::string& l_dstKeywordName =
                l_aRecordKwInfo.value("destinationKeyword", "");

            if (l_srcRecordName.empty() || l_dstRecordName.empty() ||
                l_srcKeywordName.empty() || l_dstKeywordName.empty())
            {
                // TODO: Enable logging when verbose is enabled.
                std::cout << "Record or keyword not found in the JSON."
                          << std::endl;
                continue;
            }

            types::DbusVariantType l_srcKeywordVariant;
            if (l_isSourceOnHardware)
            {
                l_srcKeywordVariant = utils::readKeywordFromHardware(
                    l_srcVpdPath,
                    std::make_tuple(l_srcRecordName, l_srcKeywordName));
            }
            else
            {
                l_srcKeywordVariant = utils::readDbusProperty(
                    constants::inventoryManagerService, l_srcVpdPath,
                    constants::ipzVpdInfPrefix + l_srcRecordName,
                    l_srcKeywordName);
            }

            if (auto l_srcKeywordValue =
                    std::get_if<types::BinaryVector>(&l_srcKeywordVariant);
                l_srcKeywordValue && !l_srcKeywordValue->empty())
            {
                l_aRecordKwInfo["sourcekeywordValue"] = *l_srcKeywordValue;
            }
            else
            {
                // TODO: Enable logging when verbose is enabled.
                std::cout
                    << "Invalid data type or empty data received, for source record: "
                    << l_srcRecordName << ", keyword: " << l_srcKeywordName
                    << std::endl;
                continue;
            }

            types::DbusVariantType l_dstKeywordVariant;
            if (l_isDestinationOnHardware)
            {
                l_dstKeywordVariant = utils::readKeywordFromHardware(
                    l_dstVpdPath,
                    std::make_tuple(l_dstRecordName, l_dstKeywordName));
            }
            else
            {
                l_dstKeywordVariant = utils::readDbusProperty(
                    constants::inventoryManagerService, l_dstVpdPath,
                    constants::ipzVpdInfPrefix + l_dstRecordName,
                    l_dstKeywordName);
            }

            if (auto l_dstKeywordValue =
                    std::get_if<types::BinaryVector>(&l_dstKeywordVariant);
                l_dstKeywordValue && !l_dstKeywordValue->empty())
            {
                l_aRecordKwInfo["destinationkeywordValue"] = *l_dstKeywordValue;
            }
            else
            {
                // TODO: Enable logging when verbose is enabled.
                std::cout
                    << "Invalid data type or empty data received, for destination record: "
                    << l_dstRecordName << ", keyword: " << l_dstKeywordName
                    << std::endl;
                continue;
            }
        }

        l_returnValue = true;
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << l_ex.what() << std::endl;
    }

    return l_returnValue;
}

nlohmann::json VpdTool::getFruTypeProperty(
    const std::string& i_objectPath) const noexcept
{
    nlohmann::json l_resultInJson = nlohmann::json::object({});
    std::vector<std::string> l_pimInfList;

    auto l_serviceInfMap = utils::GetServiceInterfacesForObject(
        i_objectPath, std::vector<std::string>{constants::inventoryItemInf});
    if (l_serviceInfMap.contains(constants::inventoryManagerService))
    {
        l_pimInfList = l_serviceInfMap[constants::inventoryManagerService];

        // iterate through the list and find
        // "xyz.openbmc_project.Inventory.Item.*"
        for (const auto& l_interface : l_pimInfList)
        {
            if (l_interface.find(constants::inventoryItemInf) !=
                    std::string::npos &&
                l_interface.length() >
                    std::string(constants::inventoryItemInf).length())
            {
                l_resultInJson.emplace("type", l_interface);
            }
        }
    }
    return l_resultInJson;
}

bool VpdTool::isFruPresent(const std::string& i_objectPath) const noexcept
{
    bool l_returnValue{false};
    try
    {
        types::DbusVariantType l_keyWordValue;

        l_keyWordValue = utils::readDbusProperty(
            constants::inventoryManagerService, i_objectPath,
            constants::inventoryItemInf, "Present");

        if (const auto l_value = std::get_if<bool>(&l_keyWordValue))
        {
            l_returnValue = *l_value;
        }
    }
    catch (const std::runtime_error& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        // std::cerr << "Failed to check \"Present\" property for FRU "
        //           << i_objectPath << " Error: " << l_ex.what() <<
        //           std::endl;
    }
    return l_returnValue;
}

void VpdTool::printFixSystemVpdOption(
    const types::UserOption& i_option) const noexcept
{
    switch (i_option)
    {
        case types::UserOption::Exit:
            std::cout << "Enter 0 => To exit successfully : ";
            break;
        case types::UserOption::UseBackupDataForAll:
            std::cout << "Enter 1 => If you choose the data on backup for all "
                         "mismatching record-keyword pairs"
                      << std::endl;
            break;
        case types::UserOption::UseSystemBackplaneDataForAll:
            std::cout << "Enter 2 => If you choose the data on primary for all "
                         "mismatching record-keyword pairs"
                      << std::endl;
            break;
        case types::UserOption::MoreOptions:
            std::cout << "Enter 3 => If you wish to explore more options"
                      << std::endl;
            break;
        case types::UserOption::UseBackupDataForCurrent:
            std::cout << "Enter 4 => If you choose the data on backup as the "
                         "right value"
                      << std::endl;
            break;
        case types::UserOption::UseSystemBackplaneDataForCurrent:
            std::cout << "Enter 5 => If you choose the data on primary as the "
                         "right value"
                      << std::endl;
            break;
        case types::UserOption::NewValueOnBoth:
            std::cout
                << "Enter 6 => If you wish to enter a new value to update "
                   "both on backup and primary"
                << std::endl;
            break;
        case types::UserOption::SkipCurrent:
            std::cout << "Enter 7 => If you wish to skip the above "
                         "record-keyword pair"
                      << std::endl;
            break;
    }
}

int VpdTool::dumpInventory(bool i_dumpTable) const noexcept
{
    int l_rc{constants::FAILURE};

    try
    {
        // get all object paths under PIM
        const auto l_objectPaths = utils::GetSubTreePaths(
            constants::baseInventoryPath, 0,
            std::vector<std::string>{constants::inventoryItemInf});

        if (!l_objectPaths.empty())
        {
            nlohmann::json l_resultInJson = nlohmann::json::array({});

            std::for_each(l_objectPaths.begin(), l_objectPaths.end(),
                          [&](const auto& l_objectPath) {
                              const auto l_fruJson =
                                  getFruProperties(l_objectPath);
                              if (!l_fruJson.empty())
                              {
                                  if (l_resultInJson.empty())
                                  {
                                      l_resultInJson += l_fruJson;
                                  }
                                  else
                                  {
                                      l_resultInJson.at(0).insert(
                                          l_fruJson.cbegin(), l_fruJson.cend());
                                  }
                              }
                          });

            if (i_dumpTable)
            {
                // create Table object
                utils::Table l_inventoryTable{};

                // columns to be populated in the Inventory table
                const std::vector<types::TableColumnNameSizePair>
                    l_tableColumns = {
                        {"FRU", 100},         {"CC", 6},  {"DR", 20},
                        {"LocationCode", 32}, {"PN", 8},  {"PrettyName", 80},
                        {"SubModel", 10},     {"SN", 15}, {"type", 60}};

                types::TableInputData l_tableData;

                // First prepare the Table Columns
                for (const auto& l_column : l_tableColumns)
                {
                    if (constants::FAILURE ==
                        l_inventoryTable.AddColumn(l_column.first,
                                                   l_column.second))
                    {
                        // TODO: Enable logging when verbose is enabled.
                        std::cerr << "Failed to add column " << l_column.first
                                  << " in Inventory Table." << std::endl;
                    }
                }

                // iterate through the json array
                for (const auto& l_fruEntry : l_resultInJson[0].items())
                {
                    // if object path ends in "unit([0-9][0-9]?)", skip adding
                    // the object path in the table
                    if (std::regex_search(l_fruEntry.key(),
                                          std::regex("unit([0-9][0-9]?)")))
                    {
                        continue;
                    }

                    std::vector<std::string> l_row;
                    for (const auto& l_column : l_tableColumns)
                    {
                        const auto& l_fruJson = l_fruEntry.value();

                        if (l_column.first == "FRU")
                        {
                            l_row.push_back(l_fruEntry.key());
                        }
                        else
                        {
                            if (l_fruJson.contains(l_column.first))
                            {
                                l_row.push_back(l_fruJson[l_column.first]);
                            }
                            else
                            {
                                l_row.push_back("");
                            }
                        }
                    }

                    l_tableData.push_back(l_row);
                }

                l_rc = l_inventoryTable.Print(l_tableData);
            }
            else
            {
                // print JSON to console
                utils::printJson(l_resultInJson);
                l_rc = constants::SUCCESS;
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << "Dump inventory failed. Error: " << l_ex.what()
                  << std::endl;
    }
    return l_rc;
}

void VpdTool::printSystemVpd(
    const nlohmann::json& i_parsedJsonObj) const noexcept
{
    if (i_parsedJsonObj.empty() || !i_parsedJsonObj.contains("backupMap"))
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << "Invalid JSON to print system VPD" << std::endl;
    }

    std::string l_outline(191, '=');
    std::cout << "\nRestorable record-keyword pairs and their data on backup & "
                 "primary.\n\n"
              << l_outline << std::endl;

    std::cout << std::left << std::setw(6) << "S.No" << std::left
              << std::setw(8) << "Record" << std::left << std::setw(9)
              << "Keyword" << std::left << std::setw(75) << "Data On Backup"
              << std::left << std::setw(75) << "Data On Primary" << std::left
              << std::setw(14) << "Data Mismatch\n"
              << l_outline << std::endl;

    uint8_t l_slNum = 0;

    for (const auto& l_aRecordKwInfo : i_parsedJsonObj["backupMap"])
    {
        if (l_aRecordKwInfo.contains("sourceRecord") ||
            l_aRecordKwInfo.contains("sourceKeyword") ||
            l_aRecordKwInfo.contains("destinationkeywordValue") ||
            l_aRecordKwInfo.contains("sourcekeywordValue"))
        {
            std::string l_mismatchFound{
                (l_aRecordKwInfo["destinationkeywordValue"] !=
                 l_aRecordKwInfo["sourcekeywordValue"])
                    ? "YES"
                    : "NO"};

            std::string l_splitLine(191, '-');

            try
            {
                std::cout << std::left << std::setw(6)
                          << static_cast<int>(++l_slNum) << std::left
                          << std::setw(8)
                          << l_aRecordKwInfo.value("sourceRecord", "")
                          << std::left << std::setw(9)
                          << l_aRecordKwInfo.value("sourceKeyword", "")
                          << std::left << std::setw(75) << std::setfill(' ')
                          << utils::getPrintableValue(
                                 l_aRecordKwInfo["destinationkeywordValue"])
                          << std::left << std::setw(75) << std::setfill(' ')
                          << utils::getPrintableValue(
                                 l_aRecordKwInfo["sourcekeywordValue"])
                          << std::left << std::setw(14) << l_mismatchFound
                          << '\n'
                          << l_splitLine << std::endl;
            }
            catch (const std::exception& l_ex)
            {
                // TODO: Enable logging when verbose is enabled.
                std::cerr << l_ex.what() << std::endl;
            }
        }
    }
}

int VpdTool::updateAllKeywords(const nlohmann::json& i_parsedJsonObj,
                               bool i_useBackupData) const noexcept
{
    int l_rc = constants::FAILURE;

    if (i_parsedJsonObj.empty() || !i_parsedJsonObj.contains("source") ||
        !i_parsedJsonObj.contains("backupMap"))
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << "Invalid JSON" << std::endl;
        return l_rc;
    }

    std::string l_srcVpdPath;
    if (auto l_vpdPath = i_parsedJsonObj["source"].value("hardwarePath", "");
        !l_vpdPath.empty())
    {
        l_srcVpdPath = l_vpdPath;
    }
    else if (auto l_vpdPath =
                 i_parsedJsonObj["source"].value("inventoryPath", "");
             !l_vpdPath.empty())
    {
        l_srcVpdPath = l_vpdPath;
    }
    else
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << "source path information is missing in JSON" << std::endl;
        return l_rc;
    }

    bool l_anyMismatchFound = false;
    for (const auto& l_aRecordKwInfo : i_parsedJsonObj["backupMap"])
    {
        if (!l_aRecordKwInfo.contains("sourceRecord") ||
            !l_aRecordKwInfo.contains("sourceKeyword") ||
            !l_aRecordKwInfo.contains("destinationkeywordValue") ||
            !l_aRecordKwInfo.contains("sourcekeywordValue"))
        {
            // TODO: Enable logging when verbose is enabled.
            std::cerr << "Missing required information in the JSON"
                      << std::endl;
            continue;
        }

        if (l_aRecordKwInfo["sourcekeywordValue"] !=
            l_aRecordKwInfo["destinationkeywordValue"])
        {
            l_anyMismatchFound = true;

            auto l_keywordValue =
                i_useBackupData ? l_aRecordKwInfo["destinationkeywordValue"]
                                : l_aRecordKwInfo["sourcekeywordValue"];

            auto l_paramsToWrite = std::make_tuple(
                l_aRecordKwInfo["sourceRecord"],
                l_aRecordKwInfo["sourceKeyword"], l_keywordValue);

            try
            {
                l_rc = utils::writeKeyword(l_srcVpdPath, l_paramsToWrite);
                if (l_rc > 0)
                {
                    l_rc = constants::SUCCESS;
                }
            }
            catch (const std::exception& l_ex)
            {
                // TODO: Enable logging when verbose is enabled.
                std::cerr << "write keyword failed for record: "
                          << l_aRecordKwInfo["sourceRecord"]
                          << ", keyword: " << l_aRecordKwInfo["sourceKeyword"]
                          << ", error: " << l_ex.what() << std::ends;
            }
        }
    }

    std::string l_dataUsed =
        (i_useBackupData ? "data from backup" : "data from primary VPD");
    if (l_anyMismatchFound)
    {
        std::cout << "Data updated successfully for all mismatching "
                     "record-keyword pairs by choosing their corresponding "
                  << l_dataUsed << ". Exit successfully." << std::endl;
    }
    else
    {
        std::cout << "No mismatch found for any of the above mentioned "
                     "record-keyword pair. Exit successfully."
                  << std::endl;
    }

    return l_rc;
}

int VpdTool::handleMoreOption(
    const nlohmann::json& i_parsedJsonObj) const noexcept
{
    int l_rc = constants::FAILURE;

    try
    {
        if (i_parsedJsonObj.empty() || !i_parsedJsonObj.contains("backupMap"))
        {
            throw std::runtime_error("Invalid JSON");
        }

        std::string l_srcVpdPath;

        if (auto l_vpdPath =
                i_parsedJsonObj["source"].value("hardwarePath", "");
            !l_vpdPath.empty())
        {
            l_srcVpdPath = l_vpdPath;
        }
        else if (auto l_vpdPath =
                     i_parsedJsonObj["source"].value("inventoryPath", "");
                 !l_vpdPath.empty())
        {
            l_srcVpdPath = l_vpdPath;
        }
        else
        {
            throw std::runtime_error(
                "source path information is missing in JSON");
        }

        auto updateKeywordValue =
            [](std::string io_vpdPath, const std::string& i_recordName,
               const std::string& i_keywordName,
               const types::BinaryVector& i_keywordValue) -> int {
            int l_rc = constants::FAILURE;

            try
            {
                auto l_paramsToWrite = std::make_tuple(
                    i_recordName, i_keywordName, i_keywordValue);
                l_rc = utils::writeKeyword(io_vpdPath, l_paramsToWrite);

                if (l_rc > 0)
                {
                    std::cout << std::endl
                              << "Data updated successfully." << std::endl;
                }
            }
            catch (const std::exception& l_ex)
            {
                // TODO: Enable log when verbose is enabled.
                std::cerr << l_ex.what() << std::endl;
            }
            return l_rc;
        };

        do
        {
            int l_slNum = 0;
            bool l_exit = false;

            for (const auto& l_aRecordKwInfo : i_parsedJsonObj["backupMap"])
            {
                if (!l_aRecordKwInfo.contains("sourceRecord") ||
                    !l_aRecordKwInfo.contains("sourceKeyword") ||
                    !l_aRecordKwInfo.contains("destinationkeywordValue") ||
                    !l_aRecordKwInfo.contains("sourcekeywordValue"))
                {
                    // TODO: Enable logging when verbose is enabled.
                    std::cerr
                        << "Source or destination information is missing in the JSON."
                        << std::endl;
                    continue;
                }

                const std::string l_mismatchFound{
                    (l_aRecordKwInfo["sourcekeywordValue"] !=
                     l_aRecordKwInfo["destinationkeywordValue"])
                        ? "YES"
                        : "NO"};

                std::cout << std::endl
                          << std::left << std::setw(6) << "S.No" << std::left
                          << std::setw(8) << "Record" << std::left
                          << std::setw(9) << "Keyword" << std::left
                          << std::setw(75) << std::setfill(' ') << "Backup Data"
                          << std::left << std::setw(75) << std::setfill(' ')
                          << "Primary Data" << std::left << std::setw(14)
                          << "Data Mismatch" << std::endl;

                std::cout << std::left << std::setw(6)
                          << static_cast<int>(++l_slNum) << std::left
                          << std::setw(8)
                          << l_aRecordKwInfo.value("sourceRecord", "")
                          << std::left << std::setw(9)
                          << l_aRecordKwInfo.value("sourceKeyword", "")
                          << std::left << std::setw(75) << std::setfill(' ')
                          << utils::getPrintableValue(
                                 l_aRecordKwInfo["destinationkeywordValue"])
                          << std::left << std::setw(75) << std::setfill(' ')
                          << utils::getPrintableValue(
                                 l_aRecordKwInfo["sourcekeywordValue"])
                          << std::left << std::setw(14) << l_mismatchFound
                          << std::endl;

                std::cout << std::string(191, '=') << std::endl;

                if (constants::STR_CMP_SUCCESS ==
                    l_mismatchFound.compare("YES"))
                {
                    printFixSystemVpdOption(
                        types::UserOption::UseBackupDataForCurrent);
                    printFixSystemVpdOption(
                        types::UserOption::UseSystemBackplaneDataForCurrent);
                    printFixSystemVpdOption(types::UserOption::NewValueOnBoth);
                    printFixSystemVpdOption(types::UserOption::SkipCurrent);
                    printFixSystemVpdOption(types::UserOption::Exit);
                }
                else
                {
                    std::cout << "No mismatch found." << std::endl << std::endl;
                    printFixSystemVpdOption(types::UserOption::NewValueOnBoth);
                    printFixSystemVpdOption(types::UserOption::SkipCurrent);
                    printFixSystemVpdOption(types::UserOption::Exit);
                }

                int l_userSelectedOption = types::UserOption::Exit;
                std::cin >> l_userSelectedOption;

                if (types::UserOption::UseBackupDataForCurrent ==
                    l_userSelectedOption)
                {
                    l_rc = updateKeywordValue(
                        l_srcVpdPath, l_aRecordKwInfo["sourceRecord"],
                        l_aRecordKwInfo["sourceKeyword"],
                        l_aRecordKwInfo["destinationkeywordValue"]);
                }
                else if (types::UserOption::UseSystemBackplaneDataForCurrent ==
                         l_userSelectedOption)
                {
                    l_rc = updateKeywordValue(
                        l_srcVpdPath, l_aRecordKwInfo["sourceRecord"],
                        l_aRecordKwInfo["sourceKeyword"],
                        l_aRecordKwInfo["sourcekeywordValue"]);
                }
                else if (types::UserOption::NewValueOnBoth ==
                         l_userSelectedOption)
                {
                    std::string l_newValue;
                    std::cout
                        << std::endl
                        << "Enter the new value to update on both "
                           "primary & backup. Value should be in ASCII or "
                           "in HEX(prefixed with 0x) : ";
                    std::cin >> l_newValue;
                    std::cout << std::endl
                              << std::string(191, '=') << std::endl;

                    try
                    {
                        l_rc = updateKeywordValue(
                            l_srcVpdPath, l_aRecordKwInfo["sourceRecord"],
                            l_aRecordKwInfo["sourceKeyword"],
                            utils::convertToBinary(l_newValue));
                    }
                    catch (const std::exception& l_ex)
                    {
                        // TODO: Enable logging when verbose is enabled.
                        std::cerr << l_ex.what() << std::endl;
                    }
                }
                else if (types::UserOption::SkipCurrent == l_userSelectedOption)
                {
                    std::cout << std::endl
                              << "Skipped the above record-keyword pair. "
                                 "Continue to the next available pair."
                              << std::endl;
                }
                else if (types::UserOption::Exit == l_userSelectedOption)
                {
                    std::cout << "Exit successfully" << std::endl;
                    l_exit = true;
                    break;
                }
                else
                {
                    std::cout << "Provide a valid option. Retrying for the "
                                 "current record-keyword pair"
                              << std::endl;
                }
            }
            if (l_exit)
            {
                l_rc = constants::SUCCESS;
                break;
            }
        } while (true);
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << l_ex.what() << std::endl;
    }

    return l_rc;
}

int VpdTool::resetVpdOnDbus()
{
    // ToDo: Limit this function to lab mode only.

    int l_rc = constants::FAILURE;
    try
    {
        std::string l_vpdManagerStopCmd(
            "systemctl stop " + std::string(constants::vpdManagerProcessName));

        std::cout << std::flush;
        if (auto l_rc = std::system(l_vpdManagerStopCmd.c_str()); l_rc != 0)
        {
            std::cerr << "Failed to stop " << constants::vpdManagerProcessName
                      << " service. Return code [" << l_rc << "]. Exiting."
                      << std::endl;
            return l_rc;
        }

        std::string l_vpdServiceIsActiveCmd(
            "systemctl is-active --quiet " +
            std::string(constants::vpdManagerProcessName));

        std::cout << std::flush;
        if (auto l_rc = std::system(l_vpdServiceIsActiveCmd.c_str()); l_rc == 0)
        {
            std::cerr
                << constants::vpdManagerProcessName
                << " service is still active, can't proceed further. Return code ["
                << l_rc << "]. Exiting." << std::endl;
            return l_rc;
        }

        std::error_code l_ec;
        if (std::filesystem::exists(INVENTORY_JSON_SYM_LINK, l_ec) &&
            !std::filesystem::remove(INVENTORY_JSON_SYM_LINK, l_ec))
        {
            std::cerr
                << "Error occured while removing the system inventory JSON sym link ["
                << INVENTORY_JSON_SYM_LINK << "]." << std::endl;

            if (l_ec)
            {
                std::cerr << "Reason: " << l_ec.message() << std::endl;
            }

            std::string l_vpdManagerStartCmd(
                "systemctl start " +
                std::string(constants::vpdManagerProcessName));

            std::cout << std::flush;
            if (auto l_rc = std::system(l_vpdManagerStartCmd.c_str());
                l_rc != 0)
            {
                std::cerr << "Failed to start "
                          << constants::vpdManagerProcessName
                          << " service. Return code [" << l_rc << "]. Exiting."
                          << std::endl
                          << "Reboot BMC to recover the system." << std::endl;
            }
            return l_rc;
        }

        if (static_cast<std::uintmax_t>(-1) ==
            std::filesystem::remove_all(constants::pimPersistPath, l_ec))
        {
            std::cerr
                << "Error occured while removing the persisted VPD under path ["
                << constants::pimPersistPath << "]." << std::endl;

            if (l_ec)
            {
                std::cerr << "Reason: " << l_ec.message() << std::endl;
            }

            std::cerr << "Reboot BMC to recover the system." << std::endl;
            return l_rc;
        }

        std::string l_pimServiceRestartCmd(
            "systemctl restart " +
            std::string(constants::inventoryManagerService));

        std::cout << std::flush;
        if (auto l_rc = std::system(l_pimServiceRestartCmd.c_str()); l_rc != 0)
        {
            std::cerr << "Failed to restart "
                      << constants::inventoryManagerService
                      << " service. Return code [" << l_rc << "]. Exiting."
                      << std::endl
                      << "Reboot BMC to recover the system." << std::endl;
            return l_rc;
        }

        std::string l_pimServiceIsActiveCmd(
            "systemctl is-active --quiet " +
            std::string(constants::inventoryManagerService));

        std::cout << std::flush;
        if (auto l_rc = std::system(l_pimServiceIsActiveCmd.c_str()); l_rc != 0)
        {
            std::cerr << constants::inventoryManagerService
                      << " service is not active. Return code [" << l_rc
                      << "]. Exiting." << std::endl
                      << "Reboot BMC to recover the system." << std::endl;
            return l_rc;
        }

        std::string l_vpdManagerStartCmd(
            "systemctl start " + std::string(constants::vpdManagerProcessName));

        std::cout << std::flush;
        if (auto l_rc = std::system(l_vpdManagerStartCmd.c_str()); l_rc != 0)
        {
            std::cerr << "Failed to start " << constants::vpdManagerProcessName
                      << " service. Return code [" << l_rc << "]. Exiting."
                      << std::endl
                      << "Reboot BMC to recover the system." << std::endl;
            return l_rc;
        }

        std::cout << std::flush;
        if (auto l_rc = std::system(l_vpdServiceIsActiveCmd.c_str()); l_rc != 0)
        {
            std::cerr << constants::vpdManagerProcessName
                      << " service is not active. Return code [" << l_rc
                      << "]. Exiting." << std::endl
                      << "Reboot BMC to recover the system." << std::endl;
            return l_rc;
        }

        std::string l_waitVpdParserStartCmd(
            "systemctl restart " +
            std::string(constants::waitVpdParserProcessName));

        std::cout << std::flush;
        if (auto l_rc = std::system(l_waitVpdParserStartCmd.c_str()); l_rc != 0)
        {
            std::cerr << "Failed to start "
                      << constants::waitVpdParserProcessName
                      << " service. Return code [" << l_rc << "]. Exiting."
                      << std::endl
                      << "Reboot BMC to recover the system." << std::endl;
            return l_rc;
        }

        l_rc = constants::SUCCESS;
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        std::cerr << l_ex.what() << std::endl;
    }

    return l_rc;
}

types::BinaryVector VpdTool::getVpdValueInBiosConfigManager(
    const std::string& i_recordName, const std::string& i_keywordName) const
{
    types::BinaryVector l_result;
    const auto l_itrToBiosAttributeKeywordMap =
        m_biosAttributeVpdKeywordMap.find(
            types::IpzType(i_recordName, i_keywordName));

    if (l_itrToBiosAttributeKeywordMap != m_biosAttributeVpdKeywordMap.end())
    {
        const auto& l_biosAttributeList =
            l_itrToBiosAttributeKeywordMap->second;
        for (const auto& l_biosAttributeEntry : l_biosAttributeList)
        {
            // get the attribute name
            const std::string l_attributeName =
                std::get<0>(l_biosAttributeEntry);

            // get the number of bits used to store the value in VPD
            const size_t l_numBitsKeyword = std::get<1>(l_biosAttributeEntry);

            auto l_attrValueVariant =
                utils::biosGetAttributeMethodCall(l_attributeName);

            if (auto l_attrVal = std::get_if<int64_t>(&l_attrValueVariant))
            {
                // multiple bytes update

                size_t l_numBytesKeyword =
                    l_numBitsKeyword / constants::VALUE_8;

                // convert to VPD format
                l_result = utils::convertIntegralTypeToBytes(*l_attrVal,
                                                             l_numBytesKeyword);
            }
            else if (auto l_attrVal =
                         std::get_if<std::string>(&l_attrValueVariant))
            {
                utils::toLower(*l_attrVal);

                // Since we are doing mfgClean, we do not
                // care about reading the current VPD keyword value before
                // writing to it.
                if (l_numBitsKeyword == constants::VALUE_1)
                {
                    // single bit update.

                    // get the bit position
                    const uint8_t l_bitPosition =
                        std::get<2>(l_biosAttributeEntry).has_value()
                            ? std::get<2>(l_biosAttributeEntry).value()
                            : constants::VALUE_0;

                    l_result.resize(constants::VALUE_1, constants::VALUE_0);

                    if (l_attrVal->compare("enabled") ==
                        constants::STR_CMP_SUCCESS)
                    {
                        l_result.at(constants::VALUE_0) |=
                            (constants::VALUE_1 << l_bitPosition);
                    }
                    else
                    {
                        l_result.at(constants::VALUE_0) &=
                            ~(constants::VALUE_1 << l_bitPosition);
                    }
                }
                else
                {
                    // single byte update
                    const auto l_enabledValue =
                        std::get<3>(l_biosAttributeEntry).has_value()
                            ? std::get<3>(l_biosAttributeEntry).value()
                            : constants::VALUE_1;

                    const auto l_disabledValue =
                        std::get<4>(l_biosAttributeEntry).has_value()
                            ? std::get<4>(l_biosAttributeEntry).value()
                            : constants::VALUE_0;

                    l_result.emplace_back((l_attrVal->compare("enabled") ==
                                           constants::STR_CMP_SUCCESS)
                                              ? l_enabledValue
                                              : l_disabledValue);
                }
            }
            else
            {
                std::cerr << "Invalid value received for attribute [" +
                                 l_attributeName + "] from BIOS Config Manager";
            }
        } // BIOS attribute loop end
    }
    return l_result;
}

void VpdTool::clearVpdDumpDir() const noexcept
{
    try
    {
        if (std::filesystem::exists(constants::badVpdPath))
        {
            std::filesystem::remove_all(constants::badVpdPath);
        }
    }
    catch (const std::exception& l_ex)
    {
        std::cerr << "Failed to clear VPD dump path:[" +
                         std::string(constants::badVpdPath) + "]. Error: "
                  << l_ex.what() << std::endl;
    }
}

} // namespace vpd
