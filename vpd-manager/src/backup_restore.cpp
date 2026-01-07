#include "backup_restore.hpp"

#include "constants.hpp"
#include "error_codes.hpp"
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
    m_sysCfgJsonObj(i_sysCfgJsonObj), m_logger(Logger::getLoggerInstance())
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

types::EepromInventoryPaths BackupAndRestore::getFruAndInvPaths(
    const std::string& i_location) const noexcept
{
    if (i_location.empty())
    {
        m_logger->logMessage("Empty location received.");
        return {};
    }

    if (!m_backupAndRestoreCfgJsonObj.contains(i_location))
    {
        m_logger->logMessage(
            i_location +
            " location is missing in the backup and restore config JSON.");
        return {};
    }

    std::string l_fruPath{};
    std::string l_invObjPath{};

    if (l_fruPath =
            m_backupAndRestoreCfgJsonObj[i_location].value("hardwarePath", "");
        !l_fruPath.empty())
    {
        uint16_t l_errCode{0};
        l_invObjPath = jsonUtility::getInventoryObjPathFromJson(
            m_sysCfgJsonObj, l_fruPath, l_errCode);

        if (l_invObjPath.empty())
        {
            std::string l_message{
                "Failed to get Dbus inventory object path for [" + i_location +
                "]."};
            if (l_errCode)
            {
                l_message.append(
                    " Error: " + commonUtility::getErrCodeMsg(l_errCode));
            }
            m_logger->logMessage(l_message);
            return {};
        }

        return std::make_tuple(l_fruPath, l_invObjPath);
    }
    else if (l_invObjPath = m_backupAndRestoreCfgJsonObj[i_location].value(
                 "inventoryPath", "");
             !l_invObjPath.empty())
    {
        uint16_t l_errCode{0};
        l_fruPath = jsonUtility::getFruPathFromJson(m_sysCfgJsonObj,
                                                    l_invObjPath, l_errCode);
        if (l_fruPath.empty())
        {
            std::string l_message{
                "Failed to get FRU path for [" + i_location + "]."};
            if (l_errCode)
            {
                l_message.append(
                    " Error: " + commonUtility::getErrCodeMsg(l_errCode));
            }
            m_logger->logMessage(l_message);
            return {};
        }

        return std::make_tuple(l_fruPath, l_invObjPath);
    }

    m_logger->logMessage(
        "Neither hardwarePath nor inventoryPath is present in the backup and restore config JSON.");
    return {};
}

std::tuple<std::string, std::string> BackupAndRestore::getSrcAndDstServiceName()
    const noexcept
{
    uint16_t l_errCode{0};

    std::string l_srcServiceName =
        jsonUtility::getServiceName(m_sysCfgJsonObj, m_srcInvPath, l_errCode);
    if (l_errCode)
    {
        m_logger->logMessage("Failed to get source service name, error : " +
                             commonUtility::getErrCodeMsg(l_errCode));

        return {};
    }

    std::string l_dstServiceName =
        jsonUtility::getServiceName(m_sysCfgJsonObj, m_dstInvPath, l_errCode);
    if (l_errCode)
    {
        m_logger->logMessage(
            "Failed to get destination service name, error : " +
            commonUtility::getErrCodeMsg(l_errCode));

        return {};
    }

    return std::make_tuple(l_srcServiceName, l_dstServiceName);
}

bool BackupAndRestore::extractAndValidateIpzRecordDetails(
    const auto& i_aRecordKwInfo,
    types::SrcDstRecordDetails o_srcDstRecordKeywordInfo,
    const std::optional<types::IPZVpdMap>& i_srcVpdMap,
    const std::optional<types::IPZVpdMap>& i_dstVpdMap) const noexcept
{
    try
    {
        auto& [l_srcRecordName, l_srcKeywordName, l_dstRecordName,
               l_dstKeywordName,
               l_defaultBinaryValue] = o_srcDstRecordKeywordInfo;

        l_srcRecordName = i_aRecordKwInfo.value("sourceRecord", "");
        l_srcKeywordName = i_aRecordKwInfo.value("sourceKeyword", "");
        l_dstRecordName = i_aRecordKwInfo.value("destinationRecord", "");
        l_dstKeywordName = i_aRecordKwInfo.value("destinationKeyword", "");

        if (l_srcRecordName.empty() || l_dstRecordName.empty() ||
            l_srcKeywordName.empty() || l_dstKeywordName.empty())
        {
            throw std::runtime_error(
                "Record or keyword not found in the backup and restore config JSON.");
        }

        if (i_srcVpdMap.has_value() && !i_srcVpdMap->empty() &&
            i_srcVpdMap->find(l_srcRecordName) == i_srcVpdMap->end())
        {
            throw std::runtime_error(
                "Record: " + l_srcRecordName + ", is not found in the source " +
                m_srcFruPath);
        }

        if (i_dstVpdMap.has_value() && !i_dstVpdMap->empty() &&
            i_dstVpdMap->find(l_dstRecordName) == i_dstVpdMap->end())
        {
            throw std::runtime_error(
                "Record: " + l_dstRecordName +
                ", is not found in the destination " + m_dstFruPath);
        }

        if (i_aRecordKwInfo.contains("defaultValue") &&
            i_aRecordKwInfo["defaultValue"].is_array())
        {
            l_defaultBinaryValue = i_aRecordKwInfo["defaultValue"]
                                       .template get<types::BinaryVector>();
        }
        else
        {
            throw std::runtime_error(
                "Couldn't read default value for record name: " +
                l_srcRecordName + ", keyword name: " + l_srcKeywordName +
                " from backup and restore config JSON file.");
        }

        return true;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to extract source and destination record details, error: " +
            std::string(l_ex.what()));
        return false;
    }
}

types::BinaryStringKwValuePair BackupAndRestore::getBinaryAndStrIpzKwValue(
    const types::IpzType& i_recordKwName, const types::IPZVpdMap& i_vpdMap,
    const std::string& i_serviceName) const noexcept
{
    try
    {
        std::string l_recordName = std::get<0>(i_recordKwName);
        std::string l_keywordName = std::get<1>(i_recordKwName);

        if (l_recordName.empty() || l_keywordName.empty() ||
            i_serviceName.empty())
        {
            throw std::runtime_error("Invalid input received.");
        }

        types::BinaryVector l_binaryValue;
        std::string l_strValue;
        if (!i_vpdMap.empty())
        {
            uint16_t l_errCode{0};
            l_strValue = vpdSpecificUtility::getKwVal(i_vpdMap.at(l_recordName),
                                                      l_keywordName, l_errCode);

            if (l_strValue.empty())
            {
                throw std::runtime_error(
                    "Keyword value not found in the given VPD map, for [" +
                    l_recordName + "][" + l_keywordName +
                    "], reason: " + commonUtility::getErrCodeMsg(l_errCode));
            }

            l_binaryValue =
                types::BinaryVector(l_strValue.begin(), l_strValue.end());
        }
        else
        {
            // Read keyword value from DBus
            const auto l_dbusValue = dbusUtility::readDbusProperty(
                i_serviceName, m_srcInvPath,
                constants::ipzVpdInf + l_recordName, l_keywordName);

            if (const auto l_value =
                    std::get_if<types::BinaryVector>(&l_dbusValue))
            {
                l_binaryValue = *l_value;
                l_strValue =
                    std::string(l_binaryValue.begin(), l_binaryValue.end());
            }
            else
            {
                throw std::runtime_error(
                    "Invalid keyword type found from Dbus, for [" +
                    l_recordName + "][" + l_keywordName + "]");
            }
        }

        return std::make_tuple(l_binaryValue, l_strValue);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to get keyword value, error: " + std::string(l_ex.what()));
        return {};
    }
}

void BackupAndRestore::syncIpzData(
    const std::string& i_fruPath, const types::IpzType& i_recordKwName,
    const types::BinaryStringKwValuePair& i_binaryStrValue,
    types::IPZVpdMap& o_vpdMap) const noexcept
{
    std::string l_recordName = std::get<0>(i_recordKwName);
    std::string l_keywordName = std::get<1>(i_recordKwName);

    types::BinaryVector l_binaryValue = std::get<0>(i_binaryStrValue);
    std::string l_strValue = std::get<1>(i_binaryStrValue);

    if (i_fruPath.empty() || l_recordName.empty() || l_keywordName.empty() ||
        l_binaryValue.empty() || l_strValue.empty())
    {
        m_logger->logMessage("Invalid input received");
        return;
    }

    // Update keyword's value on hardware
    auto l_vpdParser = std::make_shared<Parser>(i_fruPath, m_sysCfgJsonObj);

    const auto l_bytesUpdatedOnHardware = l_vpdParser->updateVpdKeyword(
        types::IpzData(l_recordName, l_keywordName, l_binaryValue));

    /* To keep the data in sync between hardware and parsed map
    updating the o_vpdMap. This should only be done if write
    on hardware returns success.*/
    if (!o_vpdMap.empty() && l_bytesUpdatedOnHardware > 0)
    {
        o_vpdMap[l_recordName][l_keywordName] = l_strValue;
    }
}

std::tuple<types::VPDMapVariant, types::VPDMapVariant>
    BackupAndRestore::backupAndRestore()
{
    auto l_emptyVariantPair =
        std::make_tuple(std::monostate{}, std::monostate{});

    if (m_backupAndRestoreStatus >= BackupAndRestoreStatus::Invoked)
    {
        m_logger->logMessage("Backup and restore invoked already.");
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
            m_logger->logMessage(
                "Backup restore config JSON is missing necessary tag(s), can't initiate backup and restore.");
            return l_emptyVariantPair;
        }

        std::tie(m_srcFruPath, m_srcInvPath) = getFruAndInvPaths("source");
        if (m_srcFruPath.empty() || m_srcInvPath.empty())
        {
            m_logger->logMessage(
                "Failed to initiate backup and restore: unable to extract source FRU or inventory path.");
            return l_emptyVariantPair;
        }

        std::tie(m_dstFruPath, m_dstInvPath) = getFruAndInvPaths("destination");
        if (m_dstFruPath.empty() || m_dstInvPath.empty())
        {
            m_logger->logMessage(
                "Failed to initiate backup and restore: unable to extract destination FRU or inventory path.");
            return l_emptyVariantPair;
        }

        types::VPDMapVariant l_srcVpdVariant;
        if (m_backupAndRestoreCfgJsonObj["source"].contains("hardwarePath"))
        {
            std::shared_ptr<Parser> l_vpdParser =
                std::make_shared<Parser>(m_srcFruPath, m_sysCfgJsonObj);
            l_srcVpdVariant = l_vpdParser->parse();
        }

        types::VPDMapVariant l_dstVpdVariant;
        if (m_backupAndRestoreCfgJsonObj["destination"].contains(
                "hardwarePath"))
        {
            std::shared_ptr<Parser> l_vpdParser =
                std::make_shared<Parser>(m_dstFruPath, m_sysCfgJsonObj);
            l_dstVpdVariant = l_vpdParser->parse();
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
                m_logger->logMessage("Source VPD is not of IPZ type.");
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
                m_logger->logMessage("Destination VPD is not of IPZ type.");
                return l_emptyVariantPair;
            }

            backupAndRestoreIpzVpd(l_srcVpdMap, l_dstVpdMap);
            m_backupAndRestoreStatus = BackupAndRestoreStatus::Completed;

            return std::make_tuple(l_srcVpdMap, l_dstVpdMap);
        }
        // Note: add implementation here to support any other VPD type.
    }
    catch (const std::exception& ex)
    {
        m_logger->logMessage("Back up and restore failed with exception: " +
                             std::string(ex.what()));
    }
    return l_emptyVariantPair;
}

void BackupAndRestore::backupAndRestoreIpzVpd(types::IPZVpdMap& io_srcVpdMap,
                                              types::IPZVpdMap& io_dstVpdMap)
{
    if (!m_backupAndRestoreCfgJsonObj["backupMap"].is_array())
    {
        m_logger->logMessage(
            "Invalid value found for tag backupMap, in backup and restore config JSON.");
        return;
    }

    if (m_srcFruPath.empty() || m_srcInvPath.empty() || m_dstFruPath.empty() ||
        m_dstInvPath.empty())
    {
        m_logger->logMessage(
            "Couldn't find either source or destination FRU or inventory path.");
        return;
    }

    auto [l_srcServiceName, l_dstServiceName] = getSrcAndDstServiceName();
    if (l_srcServiceName.empty() || l_dstServiceName.empty())
    {
        m_logger->logMessage(
            "Failed to get Dbus service name; aborting IPZ backup and restore.");
        return;
    }

    for (const auto& l_aRecordKwInfo :
         m_backupAndRestoreCfgJsonObj["backupMap"])
    {
        std::string l_srcRecordName{}, l_srcKeywordName{}, l_dstRecordName{},
            l_dstKeywordName{};
        types::BinaryVector l_defaultBinaryValue;

        if (!extractAndValidateIpzRecordDetails(
                l_aRecordKwInfo,
                std::tie(l_srcRecordName, l_srcKeywordName, l_dstRecordName,
                         l_dstKeywordName, l_defaultBinaryValue),
                io_srcVpdMap, io_dstVpdMap))
        {
            continue;
        }

        bool l_isPelRequired = l_aRecordKwInfo.value("isPelRequired", false);

        const auto [l_srcBinaryValue, l_srcStrValue] =
            getBinaryAndStrIpzKwValue(
                std::make_tuple(l_srcRecordName, l_srcKeywordName),
                io_srcVpdMap, l_srcServiceName);

        if (l_srcBinaryValue.empty() || l_srcStrValue.empty())
        {
            m_logger->logMessage(
                "Failed to get keyword value for source [" + l_srcRecordName +
                "][" + l_srcKeywordName + "]");

            continue;
        }

        const auto [l_dstBinaryValue, l_dstStrValue] =
            getBinaryAndStrIpzKwValue(
                std::make_tuple(l_dstRecordName, l_dstKeywordName),
                io_dstVpdMap, l_dstServiceName);

        if (l_dstBinaryValue.empty() || l_dstStrValue.empty())
        {
            m_logger->logMessage(
                "Failed to get keyword value for destination [" +
                l_dstRecordName + "][" + l_dstKeywordName + "]");

            continue;
        }

        if (l_srcBinaryValue != l_dstBinaryValue)
        {
            // ToDo: Handle if there is no valid default value in the backup and
            // restore config JSON.
            if (l_dstBinaryValue == l_defaultBinaryValue)
            {
                syncIpzData(m_dstFruPath,
                            std::make_tuple(l_dstRecordName, l_dstKeywordName),
                            std::make_tuple(l_srcBinaryValue, l_srcStrValue),
                            io_dstVpdMap);
                continue;
            }

            if (l_srcBinaryValue == l_defaultBinaryValue)
            {
                syncIpzData(m_srcFruPath,
                            std::make_tuple(l_srcRecordName, l_srcKeywordName),
                            std::make_tuple(l_dstBinaryValue, l_dstStrValue),
                            io_srcVpdMap);
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

int BackupAndRestore::updateKeywordOnPrimaryOrBackupPath(
    const std::string& i_fruPath,
    const types::WriteVpdParams& i_paramsToWriteData) const noexcept
{
    if (i_fruPath.empty())
    {
        m_logger->logMessage("Given FRU path is empty.");
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
                m_logger->logMessage("Invalid input received");
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
                m_logger->logMessage(
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
