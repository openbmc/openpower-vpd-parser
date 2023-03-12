#include "backup_restore.hpp"

#include "constants.hpp"
#include "event_logger.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "parser.hpp"
#include "types.hpp"

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
    try
    {
        m_backupAndRestoreCfgJsonObj =
            jsonUtility::getParsedJson(l_backupAndRestoreCfgFilePath);
    }
    catch (const std::exception& ex)
    {
        logging::logMessage(
            "Failed to intialize backup and restore object for file = " +
            l_backupAndRestoreCfgFilePath);
        throw(ex);
    }
}

std::tuple<types::VPDMapVariant, types::VPDMapVariant>
    BackupAndRestore::backupAndRestore()
{
    auto l_emptyVariantPair = std::make_tuple(std::monostate{},
                                              std::monostate{});

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
        else if (l_dstVpdPath =
                     m_backupAndRestoreCfgJsonObj["destination"].value(
                         "inventoryPath", "");
                 l_dstVpdPath.empty())
        {
            logging::logMessage(
                "Couldn't extract destination path, can't initiate backup and restore.");
            return l_emptyVariantPair;
        }

        // Implement backup and restore for IPZ type VPD
        auto l_backupAndRestoreType = m_backupAndRestoreCfgJsonObj.value("type",
                                                                         "");
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

void BackupAndRestore::backupAndRestoreIpzVpd(types::IPZVpdMap& io_srcVpdMap,
                                              types::IPZVpdMap& io_dstVpdMap,
                                              const std::string& i_srcPath,
                                              const std::string& i_dstPath)
{
    if (!m_backupAndRestoreCfgJsonObj["backupMap"].is_array())
    {
        logging::logMessage(
            "Invalid value found for tag backupMap, in backup and restore config JSON.");
        return;
    }

    const std::string l_srcFruPath =
        jsonUtility::getFruPathFromJson(m_sysCfgJsonObj, i_srcPath);
    const std::string l_dstFruPath =
        jsonUtility::getFruPathFromJson(m_sysCfgJsonObj, i_dstPath);
    if (l_srcFruPath.empty() || l_dstFruPath.empty())
    {
        logging::logMessage(
            "Couldn't find either source or destination FRU path.");
        return;
    }

    const std::string l_srcInvPath =
        jsonUtility::getInventoryObjPathFromJson(m_sysCfgJsonObj, i_srcPath);
    const std::string l_dstInvPath =
        jsonUtility::getInventoryObjPathFromJson(m_sysCfgJsonObj, i_dstPath);
    if (l_srcInvPath.empty() || l_dstInvPath.empty())
    {
        logging::logMessage(
            "Couldn't find either source or destination inventory path.");
        return;
    }

    const std::string l_srcServiceName =
        jsonUtility::getServiceName(m_sysCfgJsonObj, l_srcInvPath);
    const std::string l_dstServiceName =
        jsonUtility::getServiceName(m_sysCfgJsonObj, l_dstInvPath);
    if (l_srcServiceName.empty() || l_dstServiceName.empty())
    {
        logging::logMessage(
            "Couldn't find either source or destination DBus service name.");
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
            vpdSpecificUtility::getKwVal(io_srcVpdMap.at(l_srcRecordName),
                                         l_srcKeywordName, l_srcStrValue);
            l_srcBinaryValue = types::BinaryVector(l_srcStrValue.begin(),
                                                   l_srcStrValue.end());
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
            vpdSpecificUtility::getKwVal(io_dstVpdMap.at(l_dstRecordName),
                                         l_dstKeywordName, l_dstStrValue);
            l_dstBinaryValue = types::BinaryVector(l_dstStrValue.begin(),
                                                   l_dstStrValue.end());
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
                auto l_vpdParser = std::make_shared<Parser>(l_dstFruPath,
                                                            m_sysCfgJsonObj);

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
                auto l_vpdParser = std::make_shared<Parser>(l_srcFruPath,
                                                            m_sysCfgJsonObj);

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
                    " . Value read from source : " + l_srcStrValue +
                    " . Value read from destination : " + l_dstStrValue);

                EventLogger::createSyncPel(
                    types::ErrorType::VpdMismatch, types::SeverityType::Warning,
                    __FILE__, __FUNCTION__, 0, l_errorMsg, std::nullopt,
                    std::nullopt, std::nullopt, std::nullopt);
            }
        }
        else if (l_srcBinaryValue == l_defaultBinaryValue &&
                 l_dstBinaryValue == l_defaultBinaryValue && l_isPelRequired)
        {
            std::string l_errorMsg(
                "Default value found on both source and destination VPD, for record: " +
                l_srcRecordName + " and keyword: " + l_srcKeywordName);

            EventLogger::createSyncPel(
                types::ErrorType::DefaultValue, types::SeverityType::Error,
                __FILE__, __FUNCTION__, 0, l_errorMsg, std::nullopt,
                std::nullopt, std::nullopt, std::nullopt);
        }
    }
}

void BackupAndRestore::setBackupAndRestoreStatus(
    const BackupAndRestoreStatus& i_status)
{
    m_backupAndRestoreStatus = i_status;
}
} // namespace vpd
