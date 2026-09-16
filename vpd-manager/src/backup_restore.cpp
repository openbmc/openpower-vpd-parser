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
BackupAndRestoreStatus BackupAndRestore::backupAndRestoreStatus =
    BackupAndRestoreStatus::NotStarted;

BackupAndRestore::BackupAndRestore(const nlohmann::json& sysCfgJsonObj) :
    sysCfgJsonObj(sysCfgJsonObj), logger(Logger::getLoggerInstance())
{
    std::string backupAndRestoreCfgFilePath =
        sysCfgJsonObj.value("backupRestoreConfigPath", "");

    uint16_t errCode = 0;
    backupAndRestoreCfgJsonObj =
        jsonUtility::getParsedJson(backupAndRestoreCfgFilePath, errCode);

    if (errCode)
    {
        throw JsonException(
            "JSON parsing failed for file [" + backupAndRestoreCfgFilePath +
                "], error : " + commonUtility::getErrCodeMsg(errCode),
            backupAndRestoreCfgFilePath);
    }

    if (!isJsonValid())
    {
        throw JsonException("JSON is not valid.", backupAndRestoreCfgFilePath);
    }
}

bool BackupAndRestore::isJsonValid()
{
    if (backupAndRestoreCfgJsonObj.empty() ||
        !backupAndRestoreCfgJsonObj.contains("source") ||
        !backupAndRestoreCfgJsonObj.contains("destination") ||
        !backupAndRestoreCfgJsonObj.contains("type") ||
        !backupAndRestoreCfgJsonObj.contains("backupMap"))
    {
        logger->logMessage(
            "Backup restore config JSON is missing necessary tag(s), can't initiate backup and restore.");
        return false;
    }
    return true;
}

types::EepromInventoryPaths BackupAndRestore::getFruAndInvPaths(
    const std::string& location) const noexcept
{
    if (location.empty())
    {
        logger->logMessage("Empty location received.");
        return {};
    }

    if (!backupAndRestoreCfgJsonObj.contains(location))
    {
        logger->logMessage(
            location +
            " location is missing in the backup and restore config JSON.");
        return {};
    }

    std::string fruPath{};
    std::string invObjPath{};

    if (fruPath =
            backupAndRestoreCfgJsonObj[location].value("hardwarePath", "");
        !fruPath.empty())
    {
        uint16_t errCode{0};
        invObjPath = jsonUtility::getInventoryObjPathFromJson(fruPath, errCode);

        if (invObjPath.empty())
        {
            std::string message{
                "Failed to get Dbus inventory object path for [" + location +
                "]."};
            if (errCode)
            {
                message.append(
                    " Error: " + commonUtility::getErrCodeMsg(errCode));
            }
            logger->logMessage(message);
            return {};
        }

        return std::make_tuple(fruPath, invObjPath);
    }
    else if (invObjPath = backupAndRestoreCfgJsonObj[location].value(
                 "inventoryPath", "");
             !invObjPath.empty())
    {
        uint16_t errCode{0};
        fruPath = jsonUtility::getFruPathFromJson(invObjPath, errCode);
        if (fruPath.empty())
        {
            std::string message{
                "Failed to get FRU path for [" + location + "]."};
            if (errCode)
            {
                message.append(
                    " Error: " + commonUtility::getErrCodeMsg(errCode));
            }
            logger->logMessage(message);
            return {};
        }

        return std::make_tuple(fruPath, invObjPath);
    }

    logger->logMessage(
        "Neither hardwarePath nor inventoryPath is present in the backup and restore config JSON.");
    return {};
}

std::tuple<std::string, std::string> BackupAndRestore::getSrcAndDstServiceName()
    const noexcept
{
    uint16_t errCode{0};

    std::string srcServiceName =
        jsonUtility::getServiceName(srcInvPath, errCode);
    if (errCode)
    {
        logger->logMessage("Failed to get source service name, error : " +
                           commonUtility::getErrCodeMsg(errCode));

        return {};
    }

    std::string dstServiceName =
        jsonUtility::getServiceName(dstInvPath, errCode);
    if (errCode)
    {
        logger->logMessage("Failed to get destination service name, error : " +
                           commonUtility::getErrCodeMsg(errCode));

        return {};
    }

    return std::make_tuple(srcServiceName, dstServiceName);
}

bool BackupAndRestore::extractAndValidateIpzRecordDetails(
    const auto& aRecordKwInfo,
    types::SrcDstRecordDetails srcDstRecordKeywordInfo,
    const std::optional<types::IPZVpdMap>& srcVpdMap,
    const std::optional<types::IPZVpdMap>& dstVpdMap) const noexcept
{
    try
    {
        auto& [srcRecordName, srcKeywordName, dstRecordName, dstKeywordName,
               defaultBinaryValue] = srcDstRecordKeywordInfo;

        srcRecordName = aRecordKwInfo.value("sourceRecord", "");
        srcKeywordName = aRecordKwInfo.value("sourceKeyword", "");
        dstRecordName = aRecordKwInfo.value("destinationRecord", "");
        dstKeywordName = aRecordKwInfo.value("destinationKeyword", "");

        if (srcRecordName.empty() || dstRecordName.empty() ||
            srcKeywordName.empty() || dstKeywordName.empty())
        {
            throw std::runtime_error(
                "Record or keyword not found in the backup and restore config JSON.");
        }

        if (srcVpdMap.has_value() && !srcVpdMap->empty() &&
            srcVpdMap->find(srcRecordName) == srcVpdMap->end())
        {
            throw std::runtime_error(
                "Record: " + srcRecordName + ", is not found in the source " +
                srcFruPath);
        }

        if (dstVpdMap.has_value() && !dstVpdMap->empty() &&
            dstVpdMap->find(dstRecordName) == dstVpdMap->end())
        {
            throw std::runtime_error(
                "Record: " + dstRecordName +
                ", is not found in the destination " + dstFruPath);
        }

        if (aRecordKwInfo.contains("defaultValue") &&
            aRecordKwInfo["defaultValue"].is_array())
        {
            defaultBinaryValue = aRecordKwInfo["defaultValue"]
                                     .template get<types::BinaryVector>();
        }
        else
        {
            throw std::runtime_error(
                "Couldn't read default value for record name: " +
                srcRecordName + ", keyword name: " + srcKeywordName +
                " from backup and restore config JSON file.");
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(
            "Failed to extract source and destination record details, error: " +
            std::string(ex.what()));
        return false;
    }
}

types::BinaryStringKwValuePair BackupAndRestore::getBinaryAndStrIpzKwValue(
    const types::IpzType& recordKwName, const types::IPZVpdMap& vpdMap,
    const std::string& serviceName) const noexcept
{
    try
    {
        std::string recordName = std::get<0>(recordKwName);
        std::string keywordName = std::get<1>(recordKwName);

        if (recordName.empty() || keywordName.empty() || serviceName.empty())
        {
            throw std::runtime_error("Invalid input received.");
        }

        types::BinaryVector binaryValue;
        std::string strValue;
        if (!vpdMap.empty())
        {
            uint16_t errCode{0};
            strValue = vpdSpecificUtility::getKwVal(vpdMap.at(recordName),
                                                    keywordName, errCode);

            if (strValue.empty())
            {
                throw std::runtime_error(
                    "Keyword value not found in the given VPD map, for [" +
                    recordName + "][" + keywordName +
                    "], reason: " + commonUtility::getErrCodeMsg(errCode));
            }

            binaryValue = types::BinaryVector(strValue.begin(), strValue.end());
        }
        else
        {
            // Read keyword value from DBus
            const auto dbusValue = dbusUtility::readDbusProperty(
                serviceName, srcInvPath, constants::ipzVpdInf + recordName,
                keywordName);

            if (const auto value = std::get_if<types::BinaryVector>(&dbusValue))
            {
                binaryValue = *value;
                strValue = std::string(binaryValue.begin(), binaryValue.end());
            }
            else
            {
                throw std::runtime_error(
                    "Invalid keyword type found from Dbus, for [" + recordName +
                    "][" + keywordName + "]");
            }
        }

        return std::make_tuple(binaryValue, strValue);
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(
            "Failed to get keyword value, error: " + std::string(ex.what()));
        return {};
    }
}

void BackupAndRestore::syncIpzData(
    const std::string& fruPath, const types::IpzType& recordKwName,
    const types::BinaryStringKwValuePair& binaryStrValue,
    types::IPZVpdMap& vpdMap) const noexcept
{
    std::string recordName = std::get<0>(recordKwName);
    std::string keywordName = std::get<1>(recordKwName);

    types::BinaryVector binaryValue = std::get<0>(binaryStrValue);
    std::string strValue = std::get<1>(binaryStrValue);

    if (fruPath.empty() || recordName.empty() || keywordName.empty() ||
        binaryValue.empty() || strValue.empty())
    {
        logger->logMessage("Invalid input received");
        return;
    }

    // Update keyword's value on hardware
    auto vpdParser = std::make_shared<Parser>(fruPath, sysCfgJsonObj);

    const auto bytesUpdatedOnHardware = vpdParser->updateVpdKeyword(
        types::IpzData(recordName, keywordName, binaryValue));

    /* To keep the data in sync between hardware and parsed map
    updating the vpdMap. This should only be done if write
    on hardware returns success.*/
    if (!vpdMap.empty() && bytesUpdatedOnHardware > 0)
    {
        vpdMap[recordName][keywordName] = strValue;
    }
}

std::tuple<types::VPDMapVariant, types::VPDMapVariant>
    BackupAndRestore::backupAndRestore()
{
    auto emptyVariantPair = std::make_tuple(std::monostate{}, std::monostate{});

    try
    {
        if (backupAndRestoreStatus >= BackupAndRestoreStatus::Invoked)
        {
            throw std::runtime_error("Backup and restore invoked already.");
        }

        backupAndRestoreStatus = BackupAndRestoreStatus::Invoked;

        std::tie(srcFruPath, srcInvPath) = getFruAndInvPaths("source");
        if (srcFruPath.empty() || srcInvPath.empty())
        {
            throw std::runtime_error(
                "Failed to initiate backup and restore: unable to extract source FRU or inventory path.");
        }

        std::tie(dstFruPath, dstInvPath) = getFruAndInvPaths("destination");
        if (dstFruPath.empty() || dstInvPath.empty())
        {
            throw std::runtime_error(
                "Failed to initiate backup and restore: unable to extract destination FRU or inventory path.");
        }

        types::VPDMapVariant srcVpdVariant;
        if (backupAndRestoreCfgJsonObj["source"].contains("hardwarePath"))
        {
            std::shared_ptr<Parser> vpdParser =
                std::make_shared<Parser>(srcFruPath, sysCfgJsonObj);
            srcVpdVariant = vpdParser->parse();
        }

        types::VPDMapVariant dstVpdVariant;
        if (backupAndRestoreCfgJsonObj["destination"].contains("hardwarePath"))
        {
            std::shared_ptr<Parser> vpdParser =
                std::make_shared<Parser>(dstFruPath, sysCfgJsonObj);
            dstVpdVariant = vpdParser->parse();
        }

        // Implement backup and restore for IPZ type VPD
        auto backupAndRestoreType =
            backupAndRestoreCfgJsonObj.value("type", "");
        if (backupAndRestoreType.compare("IPZ") == constants::STR_CMP_SUCCESS)
        {
            types::IPZVpdMap srcVpdMap;
            if (auto srcVpdPtr = std::get_if<types::IPZVpdMap>(&srcVpdVariant))
            {
                srcVpdMap = *srcVpdPtr;
            }
            else if (!std::holds_alternative<std::monostate>(srcVpdVariant))
            {
                throw std::runtime_error("Source VPD is not of IPZ type.");
            }

            types::IPZVpdMap dstVpdMap;
            if (auto dstVpdPtr = std::get_if<types::IPZVpdMap>(&dstVpdVariant))
            {
                dstVpdMap = *dstVpdPtr;
            }
            else if (!std::holds_alternative<std::monostate>(dstVpdVariant))
            {
                throw std::runtime_error("Destination VPD is not of IPZ type.");
            }

            backupAndRestoreIpzVpd(srcVpdMap, dstVpdMap);
            backupAndRestoreStatus = BackupAndRestoreStatus::Completed;

            return std::make_tuple(srcVpdMap, dstVpdMap);
        }
        // Note: add implementation here to support any other VPD type.
    }
    catch (const std::exception& ex)
    {
        logger->logMessage("Back up and restore failed with exception: " +
                           std::string(ex.what()));
    }
    return emptyVariantPair;
}

void BackupAndRestore::backupAndRestoreIpzVpd(types::IPZVpdMap& srcVpdMap,
                                              types::IPZVpdMap& dstVpdMap)
{
    if (!backupAndRestoreCfgJsonObj["backupMap"].is_array())
    {
        logger->logMessage(
            "Invalid value found for tag backupMap, in backup and restore config JSON.");
        return;
    }

    if (srcFruPath.empty() || srcInvPath.empty() || dstFruPath.empty() ||
        dstInvPath.empty())
    {
        logger->logMessage(
            "Couldn't find either source or destination FRU or inventory path.");
        return;
    }

    auto [srcServiceName, dstServiceName] = getSrcAndDstServiceName();
    if (srcServiceName.empty() || dstServiceName.empty())
    {
        logger->logMessage(
            "Failed to get Dbus service name; aborting IPZ backup and restore.");
        return;
    }

    for (const auto& aRecordKwInfo : backupAndRestoreCfgJsonObj["backupMap"])
    {
        std::string srcRecordName{}, srcKeywordName{}, dstRecordName{},
            dstKeywordName{};
        types::BinaryVector defaultBinaryValue;

        if (!extractAndValidateIpzRecordDetails(
                aRecordKwInfo,
                std::tie(srcRecordName, srcKeywordName, dstRecordName,
                         dstKeywordName, defaultBinaryValue),
                srcVpdMap, dstVpdMap))
        {
            continue;
        }

        bool isPelRequired = aRecordKwInfo.value("isPelRequired", false);

        const auto [srcBinaryValue, srcStrValue] = getBinaryAndStrIpzKwValue(
            std::make_tuple(srcRecordName, srcKeywordName), srcVpdMap,
            srcServiceName);

        if (srcBinaryValue.empty() || srcStrValue.empty())
        {
            logger->logMessage("Failed to get keyword value for source [" +
                               srcRecordName + "][" + srcKeywordName + "]");

            continue;
        }

        const auto [dstBinaryValue, dstStrValue] = getBinaryAndStrIpzKwValue(
            std::make_tuple(dstRecordName, dstKeywordName), dstVpdMap,
            dstServiceName);

        if (dstBinaryValue.empty() || dstStrValue.empty())
        {
            logger->logMessage("Failed to get keyword value for destination [" +
                               dstRecordName + "][" + dstKeywordName + "]");

            continue;
        }

        if (srcBinaryValue != dstBinaryValue)
        {
            // ToDo: Handle if there is no valid default value in the backup and
            // restore config JSON.
            if (dstBinaryValue == defaultBinaryValue)
            {
                syncIpzData(
                    dstFruPath, std::make_tuple(dstRecordName, dstKeywordName),
                    std::make_tuple(srcBinaryValue, srcStrValue), dstVpdMap);
                continue;
            }

            if (srcBinaryValue == defaultBinaryValue)
            {
                syncIpzData(
                    srcFruPath, std::make_tuple(srcRecordName, srcKeywordName),
                    std::make_tuple(dstBinaryValue, dstStrValue), srcVpdMap);
            }
            else
            {
                /**
                 * Update srcVpdMap to publish the same data on DBus, which
                 * is already present on the DBus. Because after calling
                 * backupAndRestore API the map value will get published to DBus
                 * in the worker flow.
                 */
                if (!srcVpdMap.empty() && dstVpdMap.empty())
                {
                    srcVpdMap[srcRecordName][srcKeywordName] = dstStrValue;
                }

                std::string errorMsg(
                    "Mismatch found between source and destination VPD for record : " +
                    srcRecordName + " and keyword : " + srcKeywordName +
                    " . Value read from source : " +
                    commonUtility::convertByteVectorToHex(srcBinaryValue) +
                    " . Value read from destination : " +
                    commonUtility::convertByteVectorToHex(dstBinaryValue));

                logger->logMessage(errorMsg, PlaceHolder::PEL,
                                   types::PelInfoTuple{
                                       types::ErrorType::VpdMismatch,
                                       types::SeverityType::Warning, 0,
                                       std::nullopt, std::nullopt, std::nullopt,
                                       std::nullopt, std::nullopt});
            }
        }
        else if (srcBinaryValue == defaultBinaryValue &&
                 dstBinaryValue == defaultBinaryValue && isPelRequired)
        {
            std::string errorMsg(
                "Default value found on both source and destination VPD, for record: " +
                srcRecordName + " and keyword: " + srcKeywordName);

            logger->logMessage(
                errorMsg, PlaceHolder::PEL,
                types::PelInfoTuple{types::ErrorType::VpdMismatch,
                                    types::SeverityType::Warning, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt, std::nullopt});
        }
    }
}

void BackupAndRestore::setBackupAndRestoreStatus(
    const BackupAndRestoreStatus& status)
{
    backupAndRestoreStatus = status;
}

int BackupAndRestore::updateKeywordOnPrimaryOrBackupPath(
    const std::string& fruPath,
    const types::WriteVpdParams& paramsToWriteData) const noexcept
{
    if (fruPath.empty())
    {
        logger->logMessage("Given FRU path is empty.");
        return constants::FAILURE;
    }

    bool inputPathIsSourcePath = false;
    bool inputPathIsDestinationPath = false;

    if (backupAndRestoreCfgJsonObj.contains("source") &&
        backupAndRestoreCfgJsonObj["source"].value("hardwarePath", "") ==
            fruPath &&
        backupAndRestoreCfgJsonObj.contains("destination") &&
        !backupAndRestoreCfgJsonObj["destination"]
             .value("hardwarePath", "")
             .empty())
    {
        inputPathIsSourcePath = true;
    }
    else if (backupAndRestoreCfgJsonObj.contains("destination") &&
             backupAndRestoreCfgJsonObj["destination"].value("hardwarePath",
                                                             "") == fruPath &&
             backupAndRestoreCfgJsonObj.contains("source") &&
             !backupAndRestoreCfgJsonObj["source"]
                  .value("hardwarePath", "")
                  .empty())
    {
        inputPathIsDestinationPath = true;
    }
    else
    {
        // Input path is neither source or destination path of the
        // backup&restore JSON or source and destination paths are not hardware
        // paths in the config JSON.
        return constants::SUCCESS;
    }

    if (backupAndRestoreCfgJsonObj["backupMap"].is_array())
    {
        std::string inpRecordName;
        std::string inpKeywordName;
        types::BinaryVector inpKeywordValue;

        if (const types::IpzData* ipzData =
                std::get_if<types::IpzData>(&paramsToWriteData))
        {
            inpRecordName = std::get<0>(*ipzData);
            inpKeywordName = std::get<1>(*ipzData);
            inpKeywordValue = std::get<2>(*ipzData);

            if (inpRecordName.empty() || inpKeywordName.empty() ||
                inpKeywordValue.empty())
            {
                logger->logMessage("Invalid input received");
                return constants::FAILURE;
            }
        }
        else
        {
            // only IPZ type VPD is supported now.
            return constants::SUCCESS;
        }

        for (const auto& aRecordKwInfo :
             backupAndRestoreCfgJsonObj["backupMap"])
        {
            std::string srcRecordName{}, srcKeywordName{}, dstRecordName{},
                dstKeywordName{};
            types::BinaryVector defaultBinaryValue;

            if (!extractAndValidateIpzRecordDetails(
                    aRecordKwInfo,
                    std::tie(srcRecordName, srcKeywordName, dstRecordName,
                             dstKeywordName, defaultBinaryValue),
                    std::nullopt, std::nullopt))
            {
                continue;
            }

            if (inputPathIsSourcePath && (srcRecordName == inpRecordName) &&
                (srcKeywordName == inpKeywordName))
            {
                std::string fruPath(
                    backupAndRestoreCfgJsonObj["destination"]["hardwarePath"]);
                Parser parserObj(fruPath, sysCfgJsonObj);

                return parserObj.updateVpdKeyword(std::make_tuple(
                    dstRecordName, dstKeywordName, inpKeywordValue));
            }
            else if (inputPathIsDestinationPath &&
                     (dstRecordName == inpRecordName) &&
                     (dstKeywordName == inpKeywordName))
            {
                std::string fruPath(
                    backupAndRestoreCfgJsonObj["source"]["hardwarePath"]);
                Parser parserObj(fruPath, sysCfgJsonObj);

                return parserObj.updateVpdKeyword(std::make_tuple(
                    srcRecordName, srcKeywordName, inpKeywordValue));
            }
        }
    }

    // Received property is not part of backup & restore JSON.
    return constants::SUCCESS;
}

} // namespace vpd
