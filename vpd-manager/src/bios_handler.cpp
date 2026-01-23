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
    static std::shared_ptr<sdbusplus::bus::match_t> l_nameOwnerMatch =
        std::make_shared<sdbusplus::bus::match_t>(
            *m_asioConn,
            sdbusplus::bus::match::rules::nameOwnerChanged(
                constants::pldmServiceName),
            [this](sdbusplus::message_t& l_msg) {
                if (l_msg.is_method_error())
                {
                    logging::logMessage(
                        "Error in reading PLDM name owner changed signal.");
                    return;
                }

                std::string l_name;
                std::string l_newOwner;
                std::string l_oldOwner;

                l_msg.read(l_name, l_oldOwner, l_newOwner);

                if (!l_newOwner.empty() &&
                    (l_name.compare(constants::pldmServiceName) ==
                     constants::STR_CMP_SUCCESS))
                {
                    m_specificBiosHandler->backUpOrRestoreBiosAttributes();

                    // Start listener now that we have done the restore.
                    listenBiosAttributes();

                    //  We don't need the match anymore
                    l_nameOwnerMatch.reset();
                }
            });

    // Based on PLDM service status reset owner match registered above and
    // trigger BIOS attribute sync.
    if (dbusUtility::isServiceRunning(constants::pldmServiceName))
    {
        l_nameOwnerMatch.reset();
        m_specificBiosHandler->backUpOrRestoreBiosAttributes();

        // Start listener now that we have done the restore.
        listenBiosAttributes();
    }
}

template <typename T>
void BiosHandler<T>::listenBiosAttributes()
{
    static std::shared_ptr<sdbusplus::bus::match_t> l_biosMatch =
        std::make_shared<sdbusplus::bus::match_t>(
            *m_asioConn,
            sdbusplus::bus::match::rules::propertiesChanged(
                constants::biosConfigMgrObjPath,
                constants::biosConfigMgrInterface),
            [this](sdbusplus::message_t& l_msg) {
                m_specificBiosHandler->biosAttributesCallback(l_msg);
            });
}

IbmBiosHandler::IbmBiosHandler(const std::shared_ptr<Manager>& i_manager) :
    m_manager(i_manager), m_worker(i_manager->getWorkerObj() , m_logger(Logger::getLoggerInstance())
{
    if (!m_worker)
    {
        throw std::runtime_error("Worker object is null in IbmBiosHandler");
    }
    nlohmann::json l_sysCfgJsonObj{};
    l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();

    if (l_sysCfgJsonObj.empty())
    {
        throw std::runtime_error("System Configuration JSON is empty");
    }

    std::string l_backupAndRestoreCfgFilePath =
        l_sysCfgJsonObj.value("biosHandlerJsonPath", "");

    if (l_backupAndRestoreCfgFilePath.empty())
    {
        m_logger->logMessage(
            "Critical: BiosHandlerJsonPath key is missing in config.");
        throw std::runtime_error("Required configuration path is empty");
    }
    try
    {
        uint16_t l_errCode = 0;
        m_biosConfigJson = jsonUtility::getParsedJson(
            l_backupAndRestoreCfgFilePath, l_errCode);
        if (l_errCode)
        {
            throw JsonException("Failed to parse Bios Config JSON, error : " +
                                    commonUtility::getErrCodeMsg(l_errCode),
                                l_backupAndRestoreCfgFilePath);
        }
        if (m_biosConfigJson.is_null() || m_biosConfigJson.empty())
        {
            throw JsonException("Bios Config JSON is empty or invalid",
                                l_backupAndRestoreCfgFilePath);
        }
    }
    catch (const std::exception& ex)
    {
         m_logger->logMessage(
            "Parsing Bios config file failed with exception: " +
            std::string(ex.what()));
    }
}

void IbmBiosHandler::biosAttributesCallback(sdbusplus::message_t& i_msg)
{
    if (i_msg.is_method_error())
    {
        m_logger->logMessage("Error in reading BIOS attribute signal. ");
        return;
    }

    std::string l_objPath;
    types::BiosBaseTableType l_propMap;
    i_msg.read(l_objPath, l_propMap);

    for (auto l_property : l_propMap)
    {
        if (l_property.first != "BaseBIOSTable")
        {
            // Looking for change in Base BIOS table only.
            continue;
        }

        if (auto l_attributeList =
                std::get_if<std::map<std::string, types::BiosProperty>>(
                    &(l_property.second)))
        {
            for (const auto& l_attribute : *l_attributeList)
            {
                if (auto l_val = std::get_if<std::string>(
                        &(std::get<5>(std::get<1>(l_attribute)))))
                {
                    std::string l_attributeName = std::get<0>(l_attribute);
                    if (l_attributeName == "hb_memory_mirror_mode")
                    {
                        saveAmmToVpd(*l_val);
                    }

                    if (l_attributeName == "pvm_keep_and_clear")
                    {
                        saveKeepAndClearToVpd(*l_val);
                    }

                    if (l_attributeName == "pvm_create_default_lpar")
                    {
                        saveCreateDefaultLparToVpd(*l_val);
                    }

                    if (l_attributeName == "pvm_clear_nvram")
                    {
                        saveClearNvramToVpd(*l_val);
                    }

                    continue;
                }

                if (auto l_val = std::get_if<int64_t>(
                        &(std::get<5>(std::get<1>(l_attribute)))))
                {
                    std::string l_attributeName = std::get<0>(l_attribute);
                    if (l_attributeName == "hb_field_core_override")
                    {
                        saveFcoToVpd(*l_val);
                    }
                }
            }
        }
        else
        {
            m_logger->logMessage("Invalid type received for BIOS table.");
            EventLogger::createSyncPel(
                types::ErrorType::FirmwareError, types::SeverityType::Warning,
                __FILE__, __FUNCTION__, 0,
                std::string("Invalid type received for BIOS table."),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
            break;
        }
    }
}

void IbmBiosHandler::backUpOrRestoreBiosAttributes()
{
    // process FCO
    processFieldCoreOverride();

    // process AMM
    processActiveMemoryMirror();

    // process LPAR
    processCreateDefaultLpar();

    // process clear NVRAM
    processClearNvram();

    // process keep and clear
    processKeepAndClear();
}

types::BiosAttributeCurrentValue IbmBiosHandler::readBiosAttribute(
    const std::string& i_attributeName)
{
    types::BiosAttributeCurrentValue l_attrValueVariant =
        dbusUtility::biosGetAttributeMethodCall(i_attributeName);

    return l_attrValueVariant;
}

void IbmBiosHandler::processFieldCoreOverride()
{
    // TODO: Should we avoid doing this at runtime?
    std::string l_keyWordName;
    std::string l_RecordName;

    if (!m_biosConfigJson.contains("biosRecordKwMap"))
    {
        m_logger->logMessage(
            "Bios config JSON is empty or missing necessary tag(s), return");
        return;
    }

    for (const auto& entry : m_biosConfigJson["biosRecordKwMap"])
    {
        // We look for the attribute name this function is "assigned" to
        if (entry.value("biosAttributeName", "") == "hb_field_core_override")
        {
            l_RecordName = entry.value("record", "");
            l_keyWordName = entry.value("keyword", "");
            break;
        }
    }

    if (l_RecordName.empty() || l_keyWordName.empty())
    {
        m_logger->logMessage(
            "VPD mapping for hb_field_core_override not found in JSON config."
            "Skip VPD writing. ");
        return;
    }

    // Read required keyword from Dbus.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + l_RecordName, l_keyWordName);

    if (auto l_fcoInVpd = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        // default length of the keyword is 4 bytes.
        if (l_fcoInVpd->size() != constants::VALUE_4)
        {
            m_logger->logMessage(
                "Invalid value read for FCO from D-Bus. Skipping.");
        }

        //  If FCO in VPD contains anything other that ASCII Space, restore to
        //  BIOS
        if (std::any_of(l_fcoInVpd->cbegin(), l_fcoInVpd->cend(),
                        [](uint8_t l_val) {
                            return l_val != constants::ASCII_OF_SPACE;
                        }))
        {
            // Restore the data to BIOS.
            saveFcoToBios(*l_fcoInVpd);
        }
        else
        {
            types::BiosAttributeCurrentValue l_attrValueVariant =
                readBiosAttribute("hb_field_core_override");

            if (auto l_fcoInBios = std::get_if<int64_t>(&l_attrValueVariant))
            {
                // save the BIOS data to VPD
                saveFcoToVpd(*l_fcoInBios);

                return;
            }
            m_logger->logMessage("Invalid type recieved for FCO from BIOS.");
        }
        return;
    }
    m_logger->logMessage("Invalid type recieved for FCO from VPD.");
}

void IbmBiosHandler::saveFcoToVpd(int64_t i_fcoInBios)
{
    if (i_fcoInBios < 0)
    {
        logging::logMessage("Invalid FCO value in BIOS. Skip updating to VPD");
        return;
    }

    std::string l_keyWordName;
    std::string l_RecordName;

    if (!m_biosConfigJson.contains("biosRecordKwMap"))
    {
        m_logger->logMessage(
            "Bios config JSON is empty or missing necessary tag(s), return");
        return;
    }

    for (const auto& entry : m_biosConfigJson["biosRecordKwMap"])
    {
        // We look for the attribute name this function is "assigned" to
        if (entry.value("biosAttributeName", "") == "hb_field_core_override")
        {
            l_RecordName = entry.value("record", "");
            l_keyWordName = entry.value("keyword", "");
            break;
        }
    }

    // The missing attribute check
    if (l_RecordName.empty() || l_keyWordName.empty())
    {
        m_logger->logMessage(
            "VPD mapping for hb_field_core_override not found in JSON config."
            "Skip VPD writing. ");
        return;
    }

    // Read required keyword from Dbus.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::ipzVpdInf + l_RecordName, l_keyWordName);

    if (auto l_fcoInVpd = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        // default length of the keyword is 4 bytes.
        if (l_fcoInVpd->size() != constants::VALUE_4)
        {
            m_logger->logMessage(
                "Invalid value read for FCO from D-Bus. Skipping.");
            return;
        }

        // convert to VPD value type
        types::BinaryVector l_biosValInVpdFormat = {
            0, 0, 0, static_cast<uint8_t>(i_fcoInBios)};

        // Update only when the data are different.
        if (std::memcmp(l_biosValInVpdFormat.data(), l_fcoInVpd->data(),
                        constants::VALUE_4) != constants::SUCCESS)
        {
            if (constants::FAILURE ==
                m_manager->updateKeyword(
                    SYSTEM_VPD_FILE_PATH,
                    types::IpzData(constants::recVSYS, constants::kwdRG,
                                   l_biosValInVpdFormat)))
            {
                m_logger->logMessage(
                    "Failed to update " + std::string(constants::kwdRG) +
                    " keyword to VPD.");
            }
        }
    }
    else
    {
        m_logger->logMessage("Invalid type read for FCO from DBus.");
    }
}

void IbmBiosHandler::saveFcoToBios(const types::BinaryVector& i_fcoVal)
{
    if (i_fcoVal.size() != constants::VALUE_4)
    {
        m_logger->logMessage("Bad size for FCO received. Skip writing to BIOS");
        return;
    }

    types::PendingBIOSAttrs l_pendingBiosAttribute;
    l_pendingBiosAttribute.push_back(std::make_pair(
        "hb_field_core_override",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Integer",
            i_fcoVal.at(constants::VALUE_3))));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            l_pendingBiosAttribute))
    {
        // TODO: Should we log informational PEL here as well?
        m_logger->logMessage(
            "DBus call to update FCO value in pending attribute failed. ");
    }
}

void IbmBiosHandler::saveAmmToVpd(const std::string& i_memoryMirrorMode)
{
    if (i_memoryMirrorMode.empty())
    {
        logging::logMessage(
            "Empty memory mirror mode value from BIOS. Skip writing to VPD");
        return;
    }

    // Read existing value.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsysInf, constants::kwdAMM);

    if (auto l_pVal = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        auto l_ammValInVpd = *l_pVal;

        types::BinaryVector l_valToUpdateInVpd{
            (i_memoryMirrorMode == "Enabled" ? constants::AMM_ENABLED_IN_VPD
                                             : constants::AMM_DISABLED_IN_VPD)};

        // Check if value is already updated on VPD.
        if (l_ammValInVpd.at(0) == l_valToUpdateInVpd.at(0))
        {
            return;
        }

        if (constants::FAILURE ==
            m_manager->updateKeyword(
                SYSTEM_VPD_FILE_PATH,
                types::IpzData(constants::recVSYS, constants::kwdAMM,
                               l_valToUpdateInVpd)))
        {
            logging::logMessage(
                "Failed to update " + std::string(constants::kwdAMM) +
                " keyword to VPD");
        }
    }
    else
    {
        // TODO: Add PEL
        logging::logMessage(
            "Invalid type read for memory mirror mode value from DBus. Skip writing to VPD");
    }
}

void IbmBiosHandler::saveAmmToBios(const uint8_t& i_ammVal)
{
    const std::string l_valtoUpdate =
        (i_ammVal == constants::VALUE_2) ? "Enabled" : "Disabled";

    types::PendingBIOSAttrs l_pendingBiosAttribute;
    l_pendingBiosAttribute.push_back(std::make_pair(
        "hb_memory_mirror_mode",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration",
            l_valtoUpdate)));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            l_pendingBiosAttribute))
    {
        // TODO: Should we log informational PEL here as well?
        logging::logMessage(
            "DBus call to update AMM value in pending attribute failed.");
    }
}

void IbmBiosHandler::processActiveMemoryMirror()
{
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsysInf, constants::kwdAMM);

    if (auto pVal = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        auto l_ammValInVpd = *pVal;

        // Check if active memory mirror value is default in VPD.
        if (l_ammValInVpd.at(0) == constants::VALUE_0)
        {
            types::BiosAttributeCurrentValue l_attrValueVariant =
                readBiosAttribute("hb_memory_mirror_mode");

            if (auto pVal = std::get_if<std::string>(&l_attrValueVariant))
            {
                saveAmmToVpd(*pVal);
                return;
            }
            logging::logMessage(
                "Invalid type recieved for auto memory mirror mode from BIOS.");
            return;
        }
        else
        {
            saveAmmToBios(l_ammValInVpd.at(0));
        }
        return;
    }
    logging::logMessage(
        "Invalid type recieved for auto memory mirror mode from VPD.");
}

void IbmBiosHandler::saveCreateDefaultLparToVpd(
    const std::string& i_createDefaultLparVal)
{
    if (i_createDefaultLparVal.empty())
    {
        logging::logMessage(
            "Empty value received for Lpar from BIOS. Skip writing in VPD.");
        return;
    }

    // Read required keyword from DBus as we need to set only a Bit.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsysInf, constants::kwdClearNVRAM_CreateLPAR);

    if (auto l_pVal = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        commonUtility::toLower(
            const_cast<std::string&>(i_createDefaultLparVal));

        // Check for second bit. Bit set for enabled else disabled.
        if (((((*l_pVal).at(0) & 0x02) == 0x02) &&
             (i_createDefaultLparVal.compare("enabled") ==
              constants::STR_CMP_SUCCESS)) ||
            ((((*l_pVal).at(0) & 0x02) == 0x00) &&
             (i_createDefaultLparVal.compare("disabled") ==
              constants::STR_CMP_SUCCESS)))
        {
            // Values are same, Don;t update.
            return;
        }

        types::BinaryVector l_valToUpdateInVpd;
        if (i_createDefaultLparVal.compare("enabled") ==
            constants::STR_CMP_SUCCESS)
        {
            // 2nd Bit is used to store the value.
            l_valToUpdateInVpd.emplace_back((*l_pVal).at(0) | 0x02);
        }
        else
        {
            // 2nd Bit is used to store the value.
            l_valToUpdateInVpd.emplace_back((*l_pVal).at(0) & ~(0x02));
        }

        if (-1 == m_manager->updateKeyword(
                      SYSTEM_VPD_FILE_PATH,
                      types::IpzData(constants::vsysInf,
                                     constants::kwdClearNVRAM_CreateLPAR,
                                     l_valToUpdateInVpd)))
        {
            logging::logMessage(
                "Failed to update " +
                std::string(constants::kwdClearNVRAM_CreateLPAR) +
                " keyword to VPD");
        }

        return;
    }
    logging::logMessage(
        "Invalid type recieved for create default Lpar from VPD.");
}

void IbmBiosHandler::saveCreateDefaultLparToBios(
    const std::string& i_createDefaultLparVal)
{
    // checking for exact length as it is a string and can have garbage value.
    if (i_createDefaultLparVal.size() != constants::VALUE_1)
    {
        logging::logMessage(
            "Bad size for Create default LPAR in VPD. Skip writing to BIOS.");
        return;
    }

    std::string l_valtoUpdate =
        (i_createDefaultLparVal.at(0) & 0x02) ? "Enabled" : "Disabled";

    types::PendingBIOSAttrs l_pendingBiosAttribute;
    l_pendingBiosAttribute.push_back(std::make_pair(
        "pvm_create_default_lpar",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration",
            l_valtoUpdate)));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            l_pendingBiosAttribute))
    {
        logging::logMessage(
            "DBus call to update lpar value in pending attribute failed.");
    }

    return;
}

void IbmBiosHandler::processCreateDefaultLpar()
{
    // Read required keyword from DBus.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsysInf, constants::kwdClearNVRAM_CreateLPAR);

    if (auto l_pVal = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        saveCreateDefaultLparToBios(std::to_string(l_pVal->at(0)));
        return;
    }
    logging::logMessage(
        "Invalid type recieved for create default Lpar from VPD.");
}

void IbmBiosHandler::saveClearNvramToVpd(const std::string& i_clearNvramVal)
{
    if (i_clearNvramVal.empty())
    {
        logging::logMessage(
            "Empty value received for clear NVRAM from BIOS. Skip updating to VPD.");
        return;
    }

    // Read required keyword from DBus as we need to set only a Bit.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsysInf, constants::kwdClearNVRAM_CreateLPAR);

    if (auto l_pVal = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        commonUtility::toLower(const_cast<std::string&>(i_clearNvramVal));

        // Check for third bit. Bit set for enabled else disabled.
        if (((((*l_pVal).at(0) & 0x04) == 0x04) &&
             (i_clearNvramVal.compare("enabled") ==
              constants::STR_CMP_SUCCESS)) ||
            ((((*l_pVal).at(0) & 0x04) == 0x00) &&
             (i_clearNvramVal.compare("disabled") ==
              constants::STR_CMP_SUCCESS)))
        {
            // Don't update, values are same.
            return;
        }

        types::BinaryVector l_valToUpdateInVpd;
        if (i_clearNvramVal.compare("enabled") == constants::STR_CMP_SUCCESS)
        {
            // 3rd bit is used to store the value.
            l_valToUpdateInVpd.emplace_back(
                (*l_pVal).at(0) | constants::VALUE_4);
        }
        else
        {
            // 3rd bit is used to store the value.
            l_valToUpdateInVpd.emplace_back(
                (*l_pVal).at(0) & ~(constants::VALUE_4));
        }

        if (-1 == m_manager->updateKeyword(
                      SYSTEM_VPD_FILE_PATH,
                      types::IpzData(constants::vsysInf,
                                     constants::kwdClearNVRAM_CreateLPAR,
                                     l_valToUpdateInVpd)))
        {
            logging::logMessage(
                "Failed to update " +
                std::string(constants::kwdClearNVRAM_CreateLPAR) +
                " keyword to VPD");
        }

        return;
    }
    logging::logMessage("Invalid type recieved for clear NVRAM from VPD.");
}

void IbmBiosHandler::saveClearNvramToBios(const std::string& i_clearNvramVal)
{
    // Check for the exact length as it is a string and it can have a garbage
    // value.
    if (i_clearNvramVal.size() != constants::VALUE_1)
    {
        logging::logMessage(
            "Bad size for clear NVRAM in VPD. Skip writing to BIOS.");
        return;
    }

    // 3rd bit is used to store clear NVRAM value.
    std::string l_valtoUpdate =
        (i_clearNvramVal.at(0) & constants::VALUE_4) ? "Enabled" : "Disabled";

    types::PendingBIOSAttrs l_pendingBiosAttribute;
    l_pendingBiosAttribute.push_back(std::make_pair(
        "pvm_clear_nvram",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration",
            l_valtoUpdate)));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            l_pendingBiosAttribute))
    {
        logging::logMessage(
            "DBus call to update NVRAM value in pending attribute failed.");
    }
}

void IbmBiosHandler::processClearNvram()
{
    // Read required keyword from VPD.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsysInf, constants::kwdClearNVRAM_CreateLPAR);

    if (auto l_pVal = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        saveClearNvramToBios(std::to_string(l_pVal->at(0)));
        return;
    }
    logging::logMessage("Invalid type recieved for clear NVRAM from VPD.");
}

void IbmBiosHandler::saveKeepAndClearToVpd(const std::string& i_KeepAndClearVal)
{
    if (i_KeepAndClearVal.empty())
    {
        logging::logMessage(
            "Empty value received for keep and clear from BIOS. Skip updating to VPD.");
        return;
    }

    // Read required keyword from DBus as we need to set only a Bit.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsysInf, constants::kwdKeepAndClear);

    if (auto l_pVal = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        commonUtility::toLower(const_cast<std::string&>(i_KeepAndClearVal));

        // Check for first bit. Bit set for enabled else disabled.
        if (((((*l_pVal).at(0) & 0x01) == 0x01) &&
             (i_KeepAndClearVal.compare("enabled") ==
              constants::STR_CMP_SUCCESS)) ||
            ((((*l_pVal).at(0) & 0x01) == 0x00) &&
             (i_KeepAndClearVal.compare("disabled") ==
              constants::STR_CMP_SUCCESS)))
        {
            // Don't update, values are same.
            return;
        }

        types::BinaryVector l_valToUpdateInVpd;
        if (i_KeepAndClearVal.compare("enabled") == constants::STR_CMP_SUCCESS)
        {
            // 1st bit is used to store the value.
            l_valToUpdateInVpd.emplace_back(
                (*l_pVal).at(0) | constants::VALUE_1);
        }
        else
        {
            // 1st bit is used to store the value.
            l_valToUpdateInVpd.emplace_back(
                (*l_pVal).at(0) & ~(constants::VALUE_1));
        }

        if (-1 ==
            m_manager->updateKeyword(
                SYSTEM_VPD_FILE_PATH,
                types::IpzData(constants::vsysInf, constants::kwdKeepAndClear,
                               l_valToUpdateInVpd)))
        {
            logging::logMessage(
                "Failed to update " + std::string(constants::kwdKeepAndClear) +
                " keyword to VPD");
        }

        return;
    }
    logging::logMessage("Invalid type recieved for keep and clear from VPD.");
}

void IbmBiosHandler::saveKeepAndClearToBios(
    const std::string& i_KeepAndClearVal)
{
    // checking for exact length as it is a string and can have garbage value.
    if (i_KeepAndClearVal.size() != constants::VALUE_1)
    {
        logging::logMessage(
            "Bad size for keep and clear in VPD. Skip writing to BIOS.");
        return;
    }

    // 1st bit is used to store keep and clear value.
    std::string l_valtoUpdate =
        (i_KeepAndClearVal.at(0) & constants::VALUE_1) ? "Enabled" : "Disabled";

    types::PendingBIOSAttrs l_pendingBiosAttribute;
    l_pendingBiosAttribute.push_back(std::make_pair(
        "pvm_keep_and_clear",
        std::make_tuple(
            "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration",
            l_valtoUpdate)));

    if (!dbusUtility::writeDbusProperty(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "PendingAttributes",
            l_pendingBiosAttribute))
    {
        logging::logMessage(
            "DBus call to update keep and clear value in pending attribute failed.");
    }
}

void IbmBiosHandler::processKeepAndClear()
{
    // Read required keyword from VPD.
    auto l_kwdValueVariant = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsysInf, constants::kwdKeepAndClear);

    if (auto l_pVal = std::get_if<types::BinaryVector>(&l_kwdValueVariant))
    {
        saveKeepAndClearToBios(std::to_string(l_pVal->at(0)));
        return;
    }
    logging::logMessage("Invalid type recieved for keep and clear from VPD.");
}
} // namespace vpd
