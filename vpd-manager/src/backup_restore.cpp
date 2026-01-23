#include "backup_restore.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "parser.hpp"
#include "types.hpp"

#include <utility/event_logger_utility.hpp>
#include <utility/json_utility.hpp>
#include <utility/vpd_specific_utility.hpp>

namespace vpd
{
BackupAndRestoreStatus BackupAndRestore::m_backupAndRestoreStatus =
    BackupAndRestoreStatus::NotStarted;

BackupAndRestore::BackupAndRestore(const nlohmann::json& i_sysCfgJsonObj) :
    m_sysCfgJsonObj(i_sysCfgJsonObj)
{
    std::string l_backupAndRestoreCfgFilePath =
        i_sysCfgJsonObj.value("backupRestoreConfigPath", "");

    uint16_t l_errCode = 0;
    m_backupAndRestoreCfgJsonObj =
        jsonUtility::getParsedJson(l_backupAndRestoreCfgFilePath, l_errCode);

    if (l_errCode)
    {
        throw JsonException(
            "JSON parsing failed for file [" + l_backupAndRestoreCfgFilePath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode),
            l_backupAndRestoreCfgFilePath);
    }
}

std::tuple<types::VPDMapVariant, types::VPDMapVariant>
    BackupAndRestore::backupAndRestore()
{
    auto l_emptyVariantPair =
        std::make_tuple(std::monostate{}, std::monostate{});

    if (m_backupAndRestoreStatus >= BackupAndRestoreStatus::Invoked)
    {
        logging::logMessage("Backup and restore invoked already.");
        return l_emptyVariantPair;
    }

    m_backupAndRestoreStatus = BackupAndRestoreStatus::Invoked;
    try
    {
        if (m_backupAndRestoreCfgJsonObj.empty() ||
            !m_backupAndRestoreCfgJsonObj.contains("source") ||
            !m_backupAndRestoreCfgJsonObj.contains("destination") ||
            !m_backupAndRestoreCfgJsonObj.contains("type") ||
            !m_backupAndRestoreCfgJsonObj.contains("backupMap"))
        {
            logging::logMessage(
                "Backup restore config JSON is missing necessary tag(s), can't initiate backup and restore.");
            return l_emptyVariantPair;
        }

        std::string l_srcVpdPath;
        types::VPDMapVariant l_srcVpdVariant;
        if (l_srcVpdPath = m_backupAndRestoreCfgJsonObj["source"].value(
                "hardwarePath", "");
            !l_srcVpdPath.empty() && std::filesystem::exists(l_srcVpdPath))
        {
            std::shared_ptr<Parser> l_vpdParser =
                std::make_shared<Parser>(l_srcVpdPath, m_sysCfgJsonObj);
            l_srcVpdVariant = l_vpdParser->parse();
        }
        else if (l_srcVpdPath = m_backupAndRestoreCfgJsonObj["source"].value(
                     "inventoryPath", "");
                 l_srcVpdPath.empty())
        {
            logging::logMessage(
                "Couldn't extract source path, can't initiate backup and restore.");
            return l_emptyVariantPair;
        }

        std::string l_dstVpdPath;
        types::VPDMapVariant l_dstVpdVariant;
        if (l_dstVpdPath = m_backupAndRestoreCfgJsonObj["destination"].value(
                "hardwarePath", "");
            !l_dstVpdPath.empty() && std::filesystem::exists(l_dstVpdPath))
        {
            std::shared_ptr<Parser> l_vpdParser =
                std::make_shared<Parser>(l_dstVpdPath, m_sysCfgJsonObj);
            l_dstVpdVariant = l_vpdParser->parse();
        }
        else if (l_dstVpdPath = m_backupAndRestoreCfgJsonObj["destination"]
                                    .value("inventoryPath", "");
                 l_dstVpdPath.empty())
        {
            logging::logMessage(
                "Couldn't extract destination path, can't initiate backup and restore.");
            return l_emptyVariantPair;
        }

        // Implement backup and restore for IPZ type VPD
        auto l_backupAndRestoreType =
            m_backupAndRestoreCfgJsonObj.value("type", "");
        if (l_backupAndRestoreType.compare("IPZ") == constants::STR_CMP_SUCCESS)
        {
            types::IPZVpdMap l_srcVpdMap;
            if (auto l_srcVpdPtr =
                    std::get_if<types::IPZVpdMap>(&l_srcVpdVariant))
            {
                l_srcVpdMap = *l_srcVpdPtr;
            }
            else if (!std::holds_alternative<std::monostate>(l_srcVpdVariant))
            {
                logging::logMessage("Source VPD is not of IPZ type.");
                return l_emptyVariantPair;
            }

            types::IPZVpdMap l_dstVpdMap;
            if (auto l_dstVpdPtr =
                    std::get_if<types::IPZVpdMap>(&l_dstVpdVariant))
            {
                l_dstVpdMap = *l_dstVpdPtr;
            }
            else if (!std::holds_alternative<std::monostate>(l_dstVpdVariant))
            {
                logging::logMessage("Destination VPD is not of IPZ type.");
                return l_emptyVariantPair;
            }

            backupAndRestoreIpzVpd(l_srcVpdMap, l_dstVpdMap, l_srcVpdPath,
                                   l_dstVpdPath);
            m_backupAndRestoreStatus = BackupAndRestoreStatus::Completed;

            return std::make_tuple(l_srcVpdMap, l_dstVpdMap);
        }
        // Note: add implementation here to support any other VPD type.
    }
    catch (const std::exception& ex)
    {
        logging::logMessage("Back up and restore failed with exception: " +
                            std::string(ex.what()));
    }
    return l_emptyVariantPair;
}

void BackupAndRestore::backupAndRestoreIpzVpd(
    types::IPZVpdMap& io_srcVpdMap, types::IPZVpdMap& io_dstVpdMap,
    const std::string& i_srcPath, const std::string& i_dstPath)
{
    if (!m_backupAndRestoreCfgJsonObj["backupMap"].is_array())
    {
        logging::logMessage(
            "Invalid value found for tag backupMap, in backup and restore config JSON.");
        return;
    }

    uint16_t l_errCode = 0;

    const std::string l_srcFruPath =
        jsonUtility::getFruPathFromJson(m_sysCfgJsonObj, i_srcPath, l_errCode);

    if (l_errCode)
    {
        logging::logMessage(
            "Failed to get source FRU path for [" + i_srcPath +
            "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        return;
    }

    const std::string l_dstFruPath =
        jsonUtility::getFruPathFromJson(m_sysCfgJsonObj, i_dstPath, l_errCode);

    if (l_errCode)
    {
        logging::logMessage(
            "Failed to get destination FRU path for [" + i_dstPath +
            "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        return;
    }

    if (l_srcFruPath.empty() || l_dstFruPath.empty())
    {
        logging::logMessage(
            "Couldn't find either source or destination FRU path.");
        return;
    }

    const std::string l_srcInvPath = jsonUtility::getInventoryObjPathFromJson(
        m_sysCfgJsonObj, i_srcPath, l_errCode);

    if (l_srcInvPath.empty())
    {
        if (l_errCode)
        {
            logging::logMessage(
                "Couldn't find source inventory path. Error : " +
                commonUtility::getErrCodeMsg(l_errCode));
            return;
        }

        logging::logMessage("Couldn't find  source inventory path.");
        return;
    }

    const std::string l_dstInvPath = jsonUtility::getInventoryObjPathFromJson(
        m_sysCfgJsonObj, i_dstPath, l_errCode);

    if (l_dstInvPath.empty())
    {
        if (l_errCode)
        {
            logging::logMessage(
                "Couldn't find destination inventory path. Error : " +
                commonUtility::getErrCodeMsg(l_errCode));
            return;
        }

        logging::logMessage("Couldn't find destination inventory path.");
        return;
    }

    const std::string l_srcServiceName =
        jsonUtility::getServiceName(m_sysCfgJsonObj, l_srcInvPath, l_errCode);

    if (l_errCode)
    {
        logging::logMessage(
            "Failed to get service name for source FRU [" + l_srcInvPath +
            "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        return;
    }

    const std::string l_dstServiceName =
        jsonUtility::getServiceName(m_sysCfgJsonObj, l_dstInvPath, l_errCode);

    if (l_errCode)
    {
        logging::logMessage(
            "Failed to get service name for destination FRU [" + l_dstInvPath +
            "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        return;
    }

    for (const auto& l_aRecordKwInfo :
         m_backupAndRestoreCfgJsonObj["backupMap"])
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
            logging::logMessage(
                "Record or keyword not found in the backup and restore config JSON.");
            continue;
        }

        if (!io_srcVpdMap.empty() &&
            io_srcVpdMap.find(l_srcRecordName) == io_srcVpdMap.end())
        {
            logging::logMessage(
                "Record: " + l_srcRecordName +
                ", is not found in the source path: " + i_srcPath);
            continue;
        }

        if (!io_dstVpdMap.empty() &&
            io_dstVpdMap.find(l_dstRecordName) == io_dstVpdMap.end())
        {
            logging::logMessage(
                "Record: " + l_dstRecordName +
                ", is not found in the destination path: " + i_dstPath);
            continue;
        }

        types::BinaryVector l_defaultBinaryValue;
        if (l_aRecordKwInfo.contains("defaultValue") &&
            l_aRecordKwInfo["defaultValue"].is_array())
        {
            l_defaultBinaryValue =
                l_aRecordKwInfo["defaultValue"].get<types::BinaryVector>();
        }
        else
        {
            logging::logMessage(
                "Couldn't read default value for record name: " +
                l_srcRecordName + ", keyword name: " + l_srcKeywordName +
                " from backup and restore config JSON file.");
            continue;
        }

        bool l_isPelRequired = l_aRecordKwInfo.value("isPelRequired", false);

        types::BinaryVector l_srcBinaryValue;
        std::string l_srcStrValue;
        if (!io_srcVpdMap.empty())
        {
            l_srcStrValue = vpdSpecificUtility::getKwVal(
                io_srcVpdMap.at(l_srcRecordName), l_srcKeywordName, l_errCode);

            if (l_srcStrValue.empty())
            {
                std::runtime_error(
                    std::string("Failed to get value for keyword [") +
                    l_srcKeywordName + std::string("], error : ") +
                    commonUtility::getErrCodeMsg(l_errCode));
            }

            l_srcBinaryValue =
                types::BinaryVector(l_srcStrValue.begin(), l_srcStrValue.end());
        }
        else
        {
            // Read keyword value from DBus
            const auto l_value = dbusUtility::readDbusProperty(
                l_srcServiceName, l_srcInvPath,
                constants::ipzVpdInf + l_srcRecordName, l_srcKeywordName);
            if (const auto l_binaryValue =
                    std::get_if<types::BinaryVector>(&l_value))
            {
                l_srcBinaryValue = *l_binaryValue;
                l_srcStrValue = std::string(l_srcBinaryValue.begin(),
                                            l_srcBinaryValue.end());
            }
        }

        types::BinaryVector l_dstBinaryValue;
        std::string l_dstStrValue;
        if (!io_dstVpdMap.empty())
        {
            l_dstStrValue = vpdSpecificUtility::getKwVal(
                io_dstVpdMap.at(l_dstRecordName), l_dstKeywordName, l_errCode);

            if (l_dstStrValue.empty())
            {
                std::runtime_error(
                    std::string("Failed to get value for keyword [") +
                    l_dstKeywordName + std::string("], error : ") +
                    commonUtility::getErrCodeMsg(l_errCode));
            }

            l_dstBinaryValue =
                types::BinaryVector(l_dstStrValue.begin(), l_dstStrValue.end());
        }
        else
        {
            // Read keyword value from DBus
            const auto l_value = dbusUtility::readDbusProperty(
                l_dstServiceName, l_dstInvPath,
                constants::ipzVpdInf + l_dstRecordName, l_dstKeywordName);
            if (const auto l_binaryValue =
                    std::get_if<types::BinaryVector>(&l_value))
            {
                l_dstBinaryValue = *l_binaryValue;
                l_dstStrValue = std::string(l_dstBinaryValue.begin(),
                                            l_dstBinaryValue.end());
            }
        }

        if (l_srcBinaryValue != l_dstBinaryValue)
        {
            // ToDo: Handle if there is no valid default value in the backup and
            // restore config JSON.
            if (l_dstBinaryValue == l_defaultBinaryValue)
            {
                // Update keyword's value on hardware
                auto l_vpdParser =
                    std::make_shared<Parser>(l_dstFruPath, m_sysCfgJsonObj);

                auto l_bytesUpdatedOnHardware = l_vpdParser->updateVpdKeyword(
                    types::IpzData(l_dstRecordName, l_dstKeywordName,
                                   l_srcBinaryValue));

                /* To keep the data in sync between hardware and parsed map
                 updating the io_dstVpdMap. This should only be done if write
                 on hardware returns success.*/
                if (!io_dstVpdMap.empty() && l_bytesUpdatedOnHardware > 0)
                {
                    io_dstVpdMap[l_dstRecordName][l_dstKeywordName] =
                        l_srcStrValue;
                }
                continue;
            }

            if (l_srcBinaryValue == l_defaultBinaryValue)
            {
                // Update keyword's value on hardware
                auto l_vpdParser =
                    std::make_shared<Parser>(l_srcFruPath, m_sysCfgJsonObj);

                auto l_bytesUpdatedOnHardware = l_vpdParser->updateVpdKeyword(
                    types::IpzData(l_srcRecordName, l_srcKeywordName,
                                   l_dstBinaryValue));

                /* To keep the data in sync between hardware and parsed map
                 updating the io_srcVpdMap. This should only be done if write
                 on hardware returns success.*/
                if (!io_srcVpdMap.empty() && l_bytesUpdatedOnHardware > 0)
                {
                    io_srcVpdMap[l_srcRecordName][l_srcKeywordName] =
                        l_dstStrValue;
                }
            }
            else
            {
                /**
                 * Update io_srcVpdMap to publish the same data on DBus, which
                 * is already present on the DBus. Because after calling
                 * backupAndRestore API the map value will get published to DBus
                 * in the worker flow.
                 */
                if (!io_srcVpdMap.empty() && io_dstVpdMap.empty())
                {
                    io_srcVpdMap[l_srcRecordName][l_srcKeywordName] =
                        l_dstStrValue;
                }

                std::string l_errorMsg(
                    "Mismatch found between source and destination VPD for record : " +
                    l_srcRecordName + " and keyword : " + l_srcKeywordName +
                    " . Value read from source : " +
                    commonUtility::convertByteVectorToHex(l_srcBinaryValue) +
                    " . Value read from destination : " +
                    commonUtility::convertByteVectorToHex(l_dstBinaryValue));

                const types::PelInfoTuple l_pel(
                    types::ErrorType::VpdMismatch, types::SeverityType::Warning,
                    0, std::nullopt, std::nullopt, std::nullopt, std::nullopt);

                Logger::getLoggerInstance()->logMessage(
                    l_errorMsg, PlaceHolder::PEL, std::make_optional(&l_pel));
            }
        }
        else if (l_srcBinaryValue == l_defaultBinaryValue &&
                 l_dstBinaryValue == l_defaultBinaryValue && l_isPelRequired)
        {
            std::string l_errorMsg(
                "Default value found on both source and destination VPD, for record: " +
                l_srcRecordName + " and keyword: " + l_srcKeywordName);

            const types::PelInfoTuple l_pel(
                types::ErrorType::VpdMismatch, types::SeverityType::Warning, 0,
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);

            Logger::getLoggerInstance()->logMessage(
                l_errorMsg, PlaceHolder::PEL, std::make_optional(&l_pel));
        }
    }
}

void BackupAndRestore::setBackupAndRestoreStatus(
    const BackupAndRestoreStatus& i_status)
{
    m_backupAndRestoreStatus = i_status;
}

int BackupAndRestore::updateKeywordOnPrimaryOrBackupPath(
    const std::string& i_fruPath,
    const types::WriteVpdParams& i_paramsToWriteData) const noexcept
{
    if (i_fruPath.empty())
    {
        logging::logMessage("Given FRU path is empty.");
        return constants::FAILURE;
    }

    bool l_inputPathIsSourcePath = false;
    bool l_inputPathIsDestinationPath = false;

    if (m_backupAndRestoreCfgJsonObj.contains("source") &&
        m_backupAndRestoreCfgJsonObj["source"].value("hardwarePath", "") ==
            i_fruPath &&
        m_backupAndRestoreCfgJsonObj.contains("destination") &&
        !m_backupAndRestoreCfgJsonObj["destination"]
             .value("hardwarePath", "")
             .empty())
    {
        l_inputPathIsSourcePath = true;
    }
    else if (m_backupAndRestoreCfgJsonObj.contains("destination") &&
             m_backupAndRestoreCfgJsonObj["destination"].value(
                 "hardwarePath", "") == i_fruPath &&
             m_backupAndRestoreCfgJsonObj.contains("source") &&
             !m_backupAndRestoreCfgJsonObj["source"]
                  .value("hardwarePath", "")
                  .empty())
    {
        l_inputPathIsDestinationPath = true;
    }
    else
    {
        // Input path is neither source or destination path of the
        // backup&restore JSON or source and destination paths are not hardware
        // paths in the config JSON.
        return constants::SUCCESS;
    }

    if (m_backupAndRestoreCfgJsonObj["backupMap"].is_array())
    {
        std::string l_inpRecordName;
        std::string l_inpKeywordName;
        types::BinaryVector l_inpKeywordValue;

        if (const types::IpzData* l_ipzData =
                std::get_if<types::IpzData>(&i_paramsToWriteData))
        {
            l_inpRecordName = std::get<0>(*l_ipzData);
            l_inpKeywordName = std::get<1>(*l_ipzData);
            l_inpKeywordValue = std::get<2>(*l_ipzData);

            if (l_inpRecordName.empty() || l_inpKeywordName.empty() ||
                l_inpKeywordValue.empty())
            {
                logging::logMessage("Invalid input received");
                return constants::FAILURE;
            }
        }
        else
        {
            // only IPZ type VPD is supported now.
            return constants::SUCCESS;
        }

        for (const auto& l_aRecordKwInfo :
             m_backupAndRestoreCfgJsonObj["backupMap"])
        {
            if (l_aRecordKwInfo.value("sourceRecord", "").empty() ||
                l_aRecordKwInfo.value("sourceKeyword", "").empty() ||
                l_aRecordKwInfo.value("destinationRecord", "").empty() ||
                l_aRecordKwInfo.value("destinationKeyword", "").empty())
            {
                // invalid backup map found
                logging::logMessage(
                    "Invalid backup map found, one or more field(s) found empty or not present in the config JSON: sourceRecord: " +
                    l_aRecordKwInfo.value("sourceRecord", "") +
                    ", sourceKeyword: " +
                    l_aRecordKwInfo.value("sourceKeyword", "") +
                    ", destinationRecord: " +
                    l_aRecordKwInfo.value("destinationRecord", "") +
                    ", destinationKeyword: " +
                    l_aRecordKwInfo.value("destinationKeyword", ""));
                continue;
            }

            if (l_inputPathIsSourcePath &&
                (l_aRecordKwInfo["sourceRecord"] == l_inpRecordName) &&
                (l_aRecordKwInfo["sourceKeyword"] == l_inpKeywordName))
            {
                std::string l_fruPath(
                    m_backupAndRestoreCfgJsonObj["destination"]
                                                ["hardwarePath"]);
                Parser l_parserObj(l_fruPath, m_sysCfgJsonObj);

                return l_parserObj.updateVpdKeyword(std::make_tuple(
                    l_aRecordKwInfo["destinationRecord"],
                    l_aRecordKwInfo["destinationKeyword"], l_inpKeywordValue));
            }
            else if (l_inputPathIsDestinationPath &&
                     (l_aRecordKwInfo["destinationRecord"] ==
                      l_inpRecordName) &&
                     (l_aRecordKwInfo["destinationKeyword"] ==
                      l_inpKeywordName))
            {
                std::string l_fruPath(
                    m_backupAndRestoreCfgJsonObj["source"]["hardwarePath"]);
                Parser l_parserObj(l_fruPath, m_sysCfgJsonObj);

                return l_parserObj.updateVpdKeyword(std::make_tuple(
                    l_aRecordKwInfo["sourceRecord"],
                    l_aRecordKwInfo["sourceKeyword"], l_inpKeywordValue));
            }
        }
    }

    // Received property is not part of backup & restore JSON.
    return constants::SUCCESS;
}

} // namespace vpd
