#include "config.h"

#include "bios_handler.hpp"

#include "constants.hpp"
#include "logger.hpp"

#include <sdbusplus/bus/match.hpp>
#include <utility/common_utility.hpp>
#include <utility/dbus_utility.hpp>

#include <string>

namespace vpd
{
// Template declaration to define APIs.
template class BiosHandler<IbmBiosHandler>;

template <typename T>
void BiosHandler<T>::checkAndListenPldmService()
{
    // Setup a call back match on NameOwnerChanged to determine when PLDM is
    // up.
    static std::shared_ptr<sdbusplus::match> nameOwnerMatch =
        std::make_shared<sdbusplus::match>(
            *asioConn,
            sdbusplus::match_rules::nameOwnerChanged(
                constants::pldmServiceName),
            [this](sdbusplus::message_t& msg) {
                if (msg.is_method_error())
                {
                    Logger::getLoggerInstance()->logMessage(
                        "Error in reading PLDM name owner changed signal.");
                    return;
                }

                std::string name;
                std::string newOwner;
                std::string oldOwner;

                msg.read(name, oldOwner, newOwner);

                if (!newOwner.empty() &&
                    (name.compare(constants::pldmServiceName) ==
                     constants::STR_CMP_SUCCESS))
                {
                    specificBiosHandler->backUpOrRestoreBiosAttributes();

                    // Start listener now that we have done the restore.
                    listenBiosAttributes();

                    //  We don't need the match anymore
                    nameOwnerMatch.reset();
                }
            });

    // Based on PLDM service status reset owner match registered above and
    // trigger BIOS attribute sync.
    if (dbusUtility::isServiceRunning(constants::pldmServiceName))
    {
        nameOwnerMatch.reset();
        specificBiosHandler->backUpOrRestoreBiosAttributes();

        // Start listener now that we have done the restore.
        listenBiosAttributes();
    }
}

template <typename T>
void BiosHandler<T>::listenBiosAttributes()
{
    static std::shared_ptr<sdbusplus::match> biosMatch =
        std::make_shared<sdbusplus::match>(
            *asioConn,
            sdbusplus::match_rules::propertiesChanged(
                constants::biosConfigMgrObjPath,
                constants::biosConfigMgrInterface),
            [this](sdbusplus::message_t& msg) {
                specificBiosHandler->biosAttributesCallback(msg);
            });
}

IbmBiosHandler::IbmBiosHandler(const std::shared_ptr<Manager>& manager) :
    manager(manager), logger(Logger::getLoggerInstance())
{
    const std::shared_ptr<ConfigManager>& configManager =
        ConfigManager::getInstance();
    if (!configManager)
    {
        throw std::runtime_error(
            "ConfigManager object is null in IbmBiosHandler");
    }

    const auto chassisJsonResult = configManager->getJsonObj();

    if (!chassisJsonResult.has_value())
    {
        throw std::runtime_error(std::format(
            "Failed to get config JSON. Error: {}",
            commonUtility::getErrCodeMsg(chassisJsonResult.error())));
    }
    nlohmann::json sysCfgJsonObj = chassisJsonResult.value().get();

    if (sysCfgJsonObj.empty())
    {
        throw std::runtime_error("System Configuration JSON is empty");
    }

    std::string biosHandlerJsonCfgFilePath =
        sysCfgJsonObj.value("biosHandlerJsonPath", "");

    if (biosHandlerJsonCfgFilePath.empty())
    {
        throw std::runtime_error(
            "Critical: BiosHandlerJsonPath key is missing in config file.");
    }

    uint16_t errCode = 0;
    biosConfigJson =
        jsonUtility::getParsedJson(biosHandlerJsonCfgFilePath, errCode);
    if (errCode)
    {
        throw JsonException("Failed to parse Bios Config JSON, error : " +
                                commonUtility::getErrCodeMsg(errCode),
                            biosHandlerJsonCfgFilePath);
    }

    if (!biosConfigJson.contains("biosRecordKwMap"))
    {
        throw JsonException("Bios JSON is not valid.",
                            biosHandlerJsonCfgFilePath);
    }
}

void IbmBiosHandler::biosAttributesCallback(sdbusplus::message_t& msg)
{
    if (msg.is_method_error())
    {
        logger->logMessage("Error in reading BIOS attribute signal. ");
        return;
    }

    std::string objPath;
    types::BiosBaseTableType propMap;
    msg.read(objPath, propMap);

    // Build a lookup map once
    std::unordered_map<std::string, nlohmann::json> attributeConfigMap;
    for (const auto& entry : biosConfigJson["biosRecordKwMap"])
    {
        std::string attrName = entry.value("biosAttributeName", "");
        if (!attrName.empty())
        {
            attributeConfigMap[attrName] = entry;
        }
    }

    for (auto property : propMap)
    {
        if (property.first != "BaseBIOSTable")
        {
            // Looking for change in Base BIOS table only.
            continue;
        }

        if (auto attributeList =
                std::get_if<std::map<std::string, types::BiosProperty>>(
                    &(property.second)))
        {
            for (const auto& attribute : *attributeList)
            {
                std::string attributeName = std::get<0>(attribute);

                // Find config entry once
                auto configIt = attributeConfigMap.find(attributeName);
                if (configIt == attributeConfigMap.end())
                {
                    continue; // No config for this attribute
                }

                const auto& configEntry = configIt->second;

                if (auto strVal = std::get_if<std::string>(
                        &(std::get<5>(std::get<1>(attribute)))))
                {
                    if (attributeName == "hb_memory_mirror_mode")
                    {
                        saveAmmToVpd(*strVal, configEntry);
                    }

                    if (attributeName == "pvm_keep_and_clear")
                    {
                        saveKeepAndClearToVpd(*strVal, configEntry);
                    }

                    if (attributeName == "pvm_create_default_lpar")
                    {
                        saveCreateDefaultLparToVpd(*strVal, configEntry);
                    }

                    if (attributeName == "pvm_clear_nvram")
                    {
                        saveClearNvramToVpd(*strVal, configEntry);
                    }

                    continue;
                }

                if (auto val = std::get_if<int64_t>(
                        &(std::get<5>(std::get<1>(attribute)))))
                {
                    std::string attributeName = std::get<0>(attribute);
                    if (attributeName == "hb_field_core_override")
                    {
                        saveFcoToVpd(*val, configEntry);
                    }
                }
            }
        }
        else
        {
            logger->logMessage("Invalid type received for BIOS table.");

            logger->logMessage(
                std::string("Invalid type received for BIOS table."),
                PlaceHolder::PEL,
                types::PelInfoTuple{types::ErrorType::FirmwareError,
                                    types::SeverityType::Warning, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt, std::nullopt});

            break;
        }
    }
}

void IbmBiosHandler::backUpOrRestoreBiosAttributes()
{
    const std::unordered_map<std::string,
                             std::function<void(const nlohmann::json&)>>
        handlers = {
            {"hb_field_core_override",
             [this](const auto& entry) { processFieldCoreOverride(entry); }},
            {"hb_memory_mirror_mode",
             [this](const auto& entry) { processActiveMemoryMirror(entry); }},
            {"pvm_create_default_lpar",
             [this](const auto& entry) { processCreateDefaultLpar(entry); }},
            {"pvm_clear_nvram",
             [this](const auto& entry) { processClearNvram(entry); }},
            {"pvm_keep_and_clear",
             [this](const auto& entry) { processKeepAndClear(entry); }}};

    for (const auto& entry : biosConfigJson["biosRecordKwMap"])
    {
        std::string attrName = entry.value("biosAttributeName", "");
        auto it = handlers.find(attrName);
        if (it != handlers.end())
        {
            it->second(entry); // Run the respective function
        }
    }
}

types::BiosAttributeCurrentValue IbmBiosHandler::readBiosAttribute(
    const std::string& attributeName)
{
    types::BiosAttributeCurrentValue attrValueVariant =
        dbusUtility::biosGetAttributeMethodCall(attributeName);

    return attrValueVariant;
}

void IbmBiosHandler::processFieldCoreOverride(
    const nlohmann::json& attributeData)
{
    // TODO: Should we avoid doing this at runtime?
    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for hb_field_core_override not found in JSON config."
            "Skipping BIOS and VPD sync for the same.");
        return;
    }

    // Read required keyword from Dbus.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto fcoInVpd = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        // default length of the keyword is 4 bytes.
        if (fcoInVpd->size() != constants::VALUE_4)
        {
            logger->logMessage(
                "Invalid value read for FCO from D-Bus. Skipping.");
        }

        //  If FCO in VPD contains anything other that ASCII Space, restore to
        //  BIOS
        if (std::any_of(fcoInVpd->cbegin(), fcoInVpd->cend(), [](uint8_t val) {
                return val != constants::ASCII_OF_SPACE;
            }))
        {
            // Restore the data to BIOS.
            saveFcoToBios(*fcoInVpd);
        }
        else
        {
            types::BiosAttributeCurrentValue attrValueVariant =
                readBiosAttribute("hb_field_core_override");

            if (auto fcoInBios = std::get_if<int64_t>(&attrValueVariant))
            {
                // save the BIOS data to VPD
                saveFcoToVpd(*fcoInBios, attributeData);

                return;
            }
            logger->logMessage("Invalid type received for FCO from BIOS.");
        }
        return;
    }
    logger->logMessage("Invalid type received for FCO from VPD.");
}

void IbmBiosHandler::saveFcoToVpd(int64_t fcoInBios,
                                  const nlohmann::json& attributeData)
{
    if (fcoInBios < 0)
    {
        logger->logMessage("Invalid FCO value in BIOS. Skip updating to VPD");
        return;
    }

    std::string recordName = attributeData.value("record", "");
    std::string keywordName = attributeData.value("keyword", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for hb_field_core_override not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    // Read required keyword from Dbus.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto fcoInVpd = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        // default length of the keyword is 4 bytes.
        if (fcoInVpd->size() != constants::VALUE_4)
        {
            logger->logMessage(
                "Invalid value read for FCO from D-Bus. Skipping.");
            return;
        }

        // convert to VPD value type
        types::BinaryVector biosValInVpdFormat = {
            0, 0, 0, static_cast<uint8_t>(fcoInBios)};

        // Update only when the data are different.
        if (std::memcmp(biosValInVpdFormat.data(), fcoInVpd->data(),
                        constants::VALUE_4) != constants::SUCCESS)
        {
            if (constants::FAILURE ==
                manager->updateKeyword(
                    SYSTEM_VPD_FILE_PATH,
                    types::IpzData(constants::recVSYS, constants::kwdRG,
                                   biosValInVpdFormat)))
            {
                logger->logMessage(
                    "Failed to update " + std::string(constants::kwdRG) +
                    " keyword to VPD.");
            }
        }
    }
    else
    {
        logger->logMessage("Invalid type read for FCO from DBus.");
    }
}

void IbmBiosHandler::saveFcoToBios(const types::BinaryVector& fcoVal)
{
    if (fcoVal.size() != constants::VALUE_4)
    {
        logger->logMessage("Bad size for FCO received. Skip writing to BIOS");
        return;
    }

    types::PendingBIOSAttrs pendingBiosAttribute;
    pendingBiosAttribute.push_back(std::make_pair(
        "hb_field_core_override",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Integer",
            fcoVal.at(constants::VALUE_3))));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            pendingBiosAttribute))
    {
        logger->logMessage(
            "DBus call to update FCO value in pending attribute failed. ");

        logger->logMessage(
            std::string(
                "DBus call to update FCO value in pending attribute failed"),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
    }
}

void IbmBiosHandler::saveAmmToVpd(const std::string& memoryMirrorMode,
                                  const nlohmann::json& attributeData)
{
    if (memoryMirrorMode.empty())
    {
        logger->logMessage(
            "Empty memory mirror mode value from BIOS. Skip writing to VPD");
        return;
    }

    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for hb_memory_mirror_mode not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    // Read existing value.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto pVal = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        auto ammValInVpd = *pVal;

        types::BinaryVector valToUpdateInVpd{
            (memoryMirrorMode == "Enabled" ? constants::AMM_ENABLED_IN_VPD
                                           : constants::AMM_DISABLED_IN_VPD)};

        // Check if value is already updated on VPD.
        if (ammValInVpd.at(0) == valToUpdateInVpd.at(0))
        {
            return;
        }

        if (constants::FAILURE ==
            manager->updateKeyword(
                SYSTEM_VPD_FILE_PATH,
                types::IpzData(constants::recVSYS, constants::kwdAMM,
                               valToUpdateInVpd)))
        {
            logger->logMessage(
                "Failed to update " + std::string(constants::kwdAMM) +
                " keyword to VPD");
        }
    }
    else
    {
        logger->logMessage(
            "Invalid type read for memory mirror mode value from DBus. Skip writing to VPD");

        logger->logMessage(
            std::string(
                "Invalid type read for memory mirror mode value from DBus. Skip writing to VPD."),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
    }
}

void IbmBiosHandler::saveAmmToBios(const uint8_t& ammVal)
{
    const std::string valToUpdate =
        (ammVal == constants::VALUE_2) ? "Enabled" : "Disabled";

    types::PendingBIOSAttrs pendingBiosAttribute;
    pendingBiosAttribute.push_back(std::make_pair(
        "hb_memory_mirror_mode",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration",
            valToUpdate)));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            pendingBiosAttribute))
    {
        logger->logMessage(
            "DBus call to update AMM value in pending attribute failed.");

        logger->logMessage(
            std::string(
                "DBus call to update AMM value in pending attribute failed."),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
    }
}

void IbmBiosHandler::processActiveMemoryMirror(
    const nlohmann::json& attributeData)
{
    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for hb_memory_mirror_mode not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto pVal = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        auto ammValInVpd = *pVal;

        // Check if active memory mirror value is default in VPD.
        if (ammValInVpd.at(0) == constants::VALUE_0)
        {
            types::BiosAttributeCurrentValue attrValueVariant =
                readBiosAttribute("hb_memory_mirror_mode");

            if (auto pVal = std::get_if<std::string>(&attrValueVariant))
            {
                saveAmmToVpd(*pVal, attributeData);
                return;
            }
            logger->logMessage(
                "Invalid type received for auto memory mirror mode from BIOS.");
            return;
        }
        else
        {
            saveAmmToBios(ammValInVpd.at(0));
        }
        return;
    }
    logger->logMessage(
        "Invalid type received for auto memory mirror mode from VPD.");

    logger->logMessage(
        std::string(
            "Invalid type received for auto memory mirror mode from VPD."),
        PlaceHolder::PEL,
        types::PelInfoTuple{types::ErrorType::FirmwareError,
                            types::SeverityType::Informational, 0, std::nullopt,
                            std::nullopt, std::nullopt, std::nullopt,
                            std::nullopt});
}

void IbmBiosHandler::saveCreateDefaultLparToVpd(
    const std::string& createDefaultLparVal,
    const nlohmann::json& attributeData)
{
    if (createDefaultLparVal.empty())
    {
        logger->logMessage(
            "Empty value received for Lpar from BIOS. Skip writing in VPD.");
        return;
    }

    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for pvm_create_default_lpar not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    // Read required keyword from DBus as we need to set only a Bit.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto pVal = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        commonUtility::toLower(const_cast<std::string&>(createDefaultLparVal));

        // Check for second bit. Bit set for enabled else disabled.
        if (((((*pVal).at(0) & 0x02) == 0x02) &&
             (createDefaultLparVal.compare("enabled") ==
              constants::STR_CMP_SUCCESS)) ||
            ((((*pVal).at(0) & 0x02) == 0x00) &&
             (createDefaultLparVal.compare("disabled") ==
              constants::STR_CMP_SUCCESS)))
        {
            // Values are same, Don;t update.
            return;
        }

        types::BinaryVector valToUpdateInVpd;
        if (createDefaultLparVal.compare("enabled") ==
            constants::STR_CMP_SUCCESS)
        {
            // 2nd Bit is used to store the value.
            valToUpdateInVpd.emplace_back((*pVal).at(0) | 0x02);
        }
        else
        {
            // 2nd Bit is used to store the value.
            valToUpdateInVpd.emplace_back((*pVal).at(0) & ~(0x02));
        }

        if (-1 == manager->updateKeyword(
                      SYSTEM_VPD_FILE_PATH,
                      types::IpzData(constants::recVSYS,
                                     constants::kwdClearNVRAM_CreateLPAR,
                                     valToUpdateInVpd)))
        {
            logger->logMessage(
                "Failed to update " +
                std::string(constants::kwdClearNVRAM_CreateLPAR) +
                " keyword to VPD");
        }

        return;
    }
    logger->logMessage(
        "Invalid type received for create default Lpar from VPD.");
}

void IbmBiosHandler::saveCreateDefaultLparToBios(
    const std::string& createDefaultLparVal)
{
    // checking for exact length as it is a string and can have garbage value.
    if (createDefaultLparVal.size() != constants::VALUE_1)
    {
        logger->logMessage(
            "Bad size for Create default LPAR in VPD. Skip writing to BIOS.");
        return;
    }

    std::string valToUpdate =
        (createDefaultLparVal.at(0) & 0x02) ? "Enabled" : "Disabled";

    types::PendingBIOSAttrs pendingBiosAttribute;
    pendingBiosAttribute.push_back(std::make_pair(
        "pvm_create_default_lpar",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration",
            valToUpdate)));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            pendingBiosAttribute))
    {
        logger->logMessage(
            "DBus call to update lpar value in pending attribute failed.");

        logger->logMessage(
            std::string(
                "DBus call to update lpar value in pending attribute failed."),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
    }

    return;
}

void IbmBiosHandler::processCreateDefaultLpar(
    const nlohmann::json& attributeData)
{
    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for pvm_create_default_lpar not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    // Read required keyword from DBus.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto pVal = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        saveCreateDefaultLparToBios(std::to_string(pVal->at(0)));
        return;
    }
    logger->logMessage(
        "Invalid type received for create default Lpar from VPD.");
}

void IbmBiosHandler::saveClearNvramToVpd(const std::string& clearNvramVal,
                                         const nlohmann::json& attributeData)
{
    if (clearNvramVal.empty())
    {
        logger->logMessage(
            "Empty value received for clear NVRAM from BIOS. Skip updating to VPD.");
        return;
    }

    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for pvm_clear_nvram not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    // Read required keyword from DBus as we need to set only a Bit.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto pVal = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        commonUtility::toLower(const_cast<std::string&>(clearNvramVal));

        // Check for third bit. Bit set for enabled else disabled.
        if (((((*pVal).at(0) & 0x04) == 0x04) &&
             (clearNvramVal.compare("enabled") ==
              constants::STR_CMP_SUCCESS)) ||
            ((((*pVal).at(0) & 0x04) == 0x00) &&
             (clearNvramVal.compare("disabled") == constants::STR_CMP_SUCCESS)))
        {
            // Don't update, values are same.
            return;
        }

        types::BinaryVector valToUpdateInVpd;
        if (clearNvramVal.compare("enabled") == constants::STR_CMP_SUCCESS)
        {
            // 3rd bit is used to store the value.
            valToUpdateInVpd.emplace_back((*pVal).at(0) | constants::VALUE_4);
        }
        else
        {
            // 3rd bit is used to store the value.
            valToUpdateInVpd.emplace_back(
                (*pVal).at(0) & ~(constants::VALUE_4));
        }

        if (-1 == manager->updateKeyword(
                      SYSTEM_VPD_FILE_PATH,
                      types::IpzData(constants::recVSYS,
                                     constants::kwdClearNVRAM_CreateLPAR,
                                     valToUpdateInVpd)))
        {
            logger->logMessage(
                "Failed to update " +
                std::string(constants::kwdClearNVRAM_CreateLPAR) +
                " keyword to VPD");
        }

        return;
    }
    logger->logMessage("Invalid type received for clear NVRAM from VPD.");
}

void IbmBiosHandler::saveClearNvramToBios(const std::string& clearNvramVal)
{
    // Check for the exact length as it is a string and it can have a garbage
    // value.
    if (clearNvramVal.size() != constants::VALUE_1)
    {
        logger->logMessage(
            "Bad size for clear NVRAM in VPD. Skip writing to BIOS.");
        return;
    }

    // 3rd bit is used to store clear NVRAM value.
    std::string valToUpdate =
        (clearNvramVal.at(0) & constants::VALUE_4) ? "Enabled" : "Disabled";

    types::PendingBIOSAttrs pendingBiosAttribute;
    pendingBiosAttribute.push_back(std::make_pair(
        "pvm_clear_nvram",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration",
            valToUpdate)));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            pendingBiosAttribute))
    {
        logger->logMessage(
            "DBus call to update NVRAM value in pending attribute failed.");

        logger->logMessage(
            std::string(
                "DBus call to update NVRAM value in pending attribute failed."),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
    }
}

void IbmBiosHandler::processClearNvram(const nlohmann::json& attributeData)
{
    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for pvm_clear_nvram not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    // Read required keyword from VPD.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto pVal = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        saveClearNvramToBios(std::to_string(pVal->at(0)));
        return;
    }
    logger->logMessage("Invalid type received for clear NVRAM from VPD.");
}

void IbmBiosHandler::saveKeepAndClearToVpd(const std::string& keepAndClearVal,
                                           const nlohmann::json& attributeData)
{
    if (keepAndClearVal.empty())
    {
        logger->logMessage(
            "Empty value received for keep and clear from BIOS. Skip updating to VPD.");
        return;
    }

    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for pvm_keep_and_clear not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    // Read required keyword from DBus as we need to set only a Bit.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto pVal = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        commonUtility::toLower(const_cast<std::string&>(keepAndClearVal));

        // Check for first bit. Bit set for enabled else disabled.
        if (((((*pVal).at(0) & 0x01) == 0x01) &&
             (keepAndClearVal.compare("enabled") ==
              constants::STR_CMP_SUCCESS)) ||
            ((((*pVal).at(0) & 0x01) == 0x00) &&
             (keepAndClearVal.compare("disabled") ==
              constants::STR_CMP_SUCCESS)))
        {
            // Don't update, values are same.
            return;
        }

        types::BinaryVector valToUpdateInVpd;
        if (keepAndClearVal.compare("enabled") == constants::STR_CMP_SUCCESS)
        {
            // 1st bit is used to store the value.
            valToUpdateInVpd.emplace_back((*pVal).at(0) | constants::VALUE_1);
        }
        else
        {
            // 1st bit is used to store the value.
            valToUpdateInVpd.emplace_back(
                (*pVal).at(0) & ~(constants::VALUE_1));
        }

        if (-1 ==
            manager->updateKeyword(
                SYSTEM_VPD_FILE_PATH,
                types::IpzData(constants::recVSYS, constants::kwdKeepAndClear,
                               valToUpdateInVpd)))
        {
            logger->logMessage(
                "Failed to update " + std::string(constants::kwdKeepAndClear) +
                " keyword to VPD");
        }

        return;
    }
    logger->logMessage("Invalid type received for keep and clear from VPD.");
}

void IbmBiosHandler::saveKeepAndClearToBios(const std::string& keepAndClearVal)
{
    // checking for exact length as it is a string and can have garbage value.
    if (keepAndClearVal.size() != constants::VALUE_1)
    {
        logger->logMessage(
            "Bad size for keep and clear in VPD. Skip writing to BIOS.");
        return;
    }

    // 1st bit is used to store keep and clear value.
    std::string valToUpdate =
        (keepAndClearVal.at(0) & constants::VALUE_1) ? "Enabled" : "Disabled";

    types::PendingBIOSAttrs pendingBiosAttribute;
    pendingBiosAttribute.push_back(std::make_pair(
        "pvm_keep_and_clear",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration",
            valToUpdate)));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            pendingBiosAttribute))
    {
        logger->logMessage(
            "DBus call to update keep and clear value in pending attribute failed.");

        logger->logMessage(
            std::string(
                "DBus call to update keep and clear value in pending attribute failed."),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
    }
}

void IbmBiosHandler::processKeepAndClear(const nlohmann::json& attributeData)
{
    std::string keywordName = attributeData.value("keyword", "");
    std::string recordName = attributeData.value("record", "");

    // The missing attribute check
    if (recordName.empty() || keywordName.empty())
    {
        logger->logMessage(
            "VPD mapping for pvm_keep_and_clear not found in JSON config."
            "Skipping BIOS and VPD sync for the same. ");
        return;
    }

    // Read required keyword from VPD.
    auto kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + recordName, keywordName);

    if (auto pVal = std::get_if<types::BinaryVector>(&kwdValueVariant))
    {
        saveKeepAndClearToBios(std::to_string(pVal->at(0)));
        return;
    }
    logger->logMessage("Invalid type received for keep and clear from VPD.");
}
} // namespace vpd
