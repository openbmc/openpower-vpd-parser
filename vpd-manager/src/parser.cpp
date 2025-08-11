#include "parser.hpp"

#include "constants.hpp"
#include "event_logger.hpp"
#ifdef VPD_WRITE_SANITY_CHECK
#include <utility/common_utility.hpp>
#endif
#include <utility/dbus_utility.hpp>
#include <utility/json_utility.hpp>
#include <utility/vpd_specific_utility.hpp>

#include <fstream>

namespace vpd
{
Parser::Parser(const std::string& vpdFilePath, nlohmann::json parsedJson) :
    m_vpdFilePath(vpdFilePath), m_parsedJson(parsedJson)
{
    std::error_code l_errCode;

    // ToDo: Add minimum file size check in all the concert praser classes,
    // depends on their VPD type.
    if (!std::filesystem::exists(m_vpdFilePath, l_errCode))
    {
        std::string l_message{"Parser object creation failed, file [" +
                              m_vpdFilePath + "] doesn't exists."};

        if (l_errCode)
        {
            l_message += " Error message: " + l_errCode.message();
        }

        throw std::runtime_error(l_message);
    }

    // Read VPD offset if applicable.
    if (!m_parsedJson.empty())
    {
        m_vpdStartOffset = jsonUtility::getVPDOffset(m_parsedJson, vpdFilePath);
    }
}

std::shared_ptr<vpd::ParserInterface> Parser::getVpdParserInstance()
{
    // Read the VPD data into a vector.
    vpdSpecificUtility::getVpdDataInVector(m_vpdFilePath, m_vpdVector,
                                           m_vpdStartOffset);

    // This will detect the type of parser required.
    std::shared_ptr<vpd::ParserInterface> l_parser =
        ParserFactory::getParser(m_vpdVector, m_vpdFilePath, m_vpdStartOffset);

    return l_parser;
}

types::VPDMapVariant Parser::parse()
{
    std::shared_ptr<vpd::ParserInterface> l_parser = getVpdParserInstance();
    return l_parser->parse();
}

int Parser::updateVpdKeyword(const types::WriteVpdParams& i_paramsToWriteData)
{
    int l_bytesUpdatedOnHardware = constants::FAILURE;

    // A lambda to extract Record : Keyword string from i_paramsToWriteData
    auto l_keyWordIdentifier =
        [](const types::WriteVpdParams& i_paramsToWriteData) -> std::string {
        std::string l_keywordString{};
        if (const types::IpzData* l_ipzData =
                std::get_if<types::IpzData>(&i_paramsToWriteData))
        {
            l_keywordString =
                std::get<0>(*l_ipzData) + ":" + std::get<1>(*l_ipzData);
        }
        else if (const types::KwData* l_kwData =
                     std::get_if<types::KwData>(&i_paramsToWriteData))
        {
            l_keywordString = std::get<0>(*l_kwData);
        }
        return l_keywordString;
    };

    try
    {
        // Enable Reboot Guard
        if (constants::FAILURE == dbusUtility::EnableRebootGuard())
        {
            EventLogger::createAsyncPel(
                types::ErrorType::DbusFailure,
                types::SeverityType::Informational, __FILE__, __FUNCTION__, 0,
                std::string(
                    "Failed to enable BMC Reboot Guard while updating " +
                    l_keyWordIdentifier(i_paramsToWriteData)),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);

            return constants::FAILURE;
        }

        // Update keyword's value on hardware
        try
        {
#ifdef VPD_WRITE_SANITY_CHECK
            checkVpdWriteSanity(i_paramsToWriteData);
#endif
            std::shared_ptr<ParserInterface> l_vpdParserInstance =
                getVpdParserInstance();
            l_bytesUpdatedOnHardware =
                l_vpdParserInstance->writeKeywordOnHardware(
                    i_paramsToWriteData);
        }
        catch (const std::exception& l_exception)
        {
            std::string l_errMsg(
                "Error while updating keyword's value on hardware path " +
                m_vpdFilePath + ", error: " + std::string(l_exception.what()));

            // TODO : Log PEL

            throw std::runtime_error(l_errMsg);
        }

        auto [l_fruPath, l_inventoryObjPath, l_redundantFruPath] =
            jsonUtility::getAllPathsToUpdateKeyword(m_parsedJson,
                                                    m_vpdFilePath);

        // If inventory D-bus object path is present, update keyword's value on
        // DBus
        if (!l_inventoryObjPath.empty())
        {
            types::Record l_recordName;
            std::string l_interfaceName;
            std::string l_propertyName;
            types::DbusVariantType l_keywordValue;

            if (const types::IpzData* l_ipzData =
                    std::get_if<types::IpzData>(&i_paramsToWriteData))
            {
                l_recordName = std::get<0>(*l_ipzData);
                l_interfaceName = constants::ipzVpdInf + l_recordName;
                l_propertyName = std::get<1>(*l_ipzData);

                try
                {
                    // Read keyword's value from hardware to write the same on
                    // D-bus.
                    std::shared_ptr<ParserInterface> l_vpdParserInstance =
                        getVpdParserInstance();

                    l_keywordValue =
                        l_vpdParserInstance->readKeywordFromHardware(
                            types::ReadVpdParams(
                                std::make_tuple(l_recordName, l_propertyName)));
                }
                catch (const std::exception& l_exception)
                {
                    // Unable to read keyword's value from hardware.
                    std::string l_errMsg(
                        "Error while reading keyword's value from hadware path " +
                        m_vpdFilePath +
                        ", error: " + std::string(l_exception.what()));

                    // TODO: Log PEL

                    throw std::runtime_error(l_errMsg);
                }
            }
            else
            {
                // Input parameter type provided isn't compatible to perform
                // update.
                std::string l_errMsg(
                    "Input parameter type isn't compatible to update keyword's value on DBus for object path: " +
                    l_inventoryObjPath);
                throw std::runtime_error(l_errMsg);
            }

            // Get D-bus name for the given keyword
            l_propertyName =
                vpdSpecificUtility::getDbusPropNameForGivenKw(l_propertyName);

            // Create D-bus object map
            types::ObjectMap l_dbusObjMap = {std::make_pair(
                l_inventoryObjPath,
                types::InterfaceMap{std::make_pair(
                    l_interfaceName, types::PropertyMap{std::make_pair(
                                         l_propertyName, l_keywordValue)})})};

            // Call PIM's Notify method to perform update
            if (!dbusUtility::callPIM(std::move(l_dbusObjMap)))
            {
                // Call to PIM's Notify method failed.
                std::string l_errMsg("Notify PIM is failed for object path: " +
                                     l_inventoryObjPath);
                throw std::runtime_error(l_errMsg);
            }
        }

        // Update keyword's value on redundant hardware if present
        if (!l_redundantFruPath.empty())
        {
            if (updateVpdKeywordOnRedundantPath(l_redundantFruPath,
                                                i_paramsToWriteData) < 0)
            {
                std::string l_errMsg(
                    "Error while updating keyword's value on redundant path " +
                    l_redundantFruPath);
                throw std::runtime_error(l_errMsg);
            }
        }

#ifdef VPD_WRITE_SANITY_CHECK
        checkVpdWriteSanity(i_paramsToWriteData);
#endif

        // TODO: Check if revert is required when any of the writes fails.
        // TODO: Handle error logging
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Update VPD Keyword failed for : " +
                            l_keyWordIdentifier(i_paramsToWriteData) +
                            " failed due to error: " + l_ex.what());

        // update failed, set return value to failure
        l_bytesUpdatedOnHardware = constants::FAILURE;
    }

    // Disable Reboot Guard
    if (constants::FAILURE == dbusUtility::DisableRebootGuard())
    {
        EventLogger::createAsyncPel(
            types::ErrorType::DbusFailure, types::SeverityType::Critical,
            __FILE__, __FUNCTION__, 0,
            std::string("Failed to disable BMC Reboot Guard while updating " +
                        l_keyWordIdentifier(i_paramsToWriteData)),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }

    return l_bytesUpdatedOnHardware;
}

int Parser::updateVpdKeywordOnRedundantPath(
    const std::string& i_fruPath,
    const types::WriteVpdParams& i_paramsToWriteData)
{
    try
    {
        std::shared_ptr<Parser> l_parserObj =
            std::make_shared<Parser>(i_fruPath, m_parsedJson);

        std::shared_ptr<ParserInterface> l_vpdParserInstance =
            l_parserObj->getVpdParserInstance();

        return l_vpdParserInstance->writeKeywordOnHardware(i_paramsToWriteData);
    }
    catch (const std::exception& l_exception)
    {
        EventLogger::createSyncPel(
            types::ErrorType::InvalidVpdMessage,
            types::SeverityType::Informational, __FILE__, __FUNCTION__, 0,
            "Error while updating keyword's value on redundant path " +
                i_fruPath + ", error: " + std::string(l_exception.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        return -1;
    }
}

int Parser::updateVpdKeywordOnHardware(
    const types::WriteVpdParams& i_paramsToWriteData)
{
    int l_bytesUpdatedOnHardware = constants::FAILURE;

    // A lambda to extract Record : Keyword string from i_paramsToWriteData
    auto l_keyWordIdentifier =
        [](const types::WriteVpdParams& i_paramsToWriteData) -> std::string {
        std::string l_keywordString{};
        if (const types::IpzData* l_ipzData =
                std::get_if<types::IpzData>(&i_paramsToWriteData))
        {
            l_keywordString =
                std::get<0>(*l_ipzData) + ":" + std::get<1>(*l_ipzData);
        }
        else if (const types::KwData* l_kwData =
                     std::get_if<types::KwData>(&i_paramsToWriteData))
        {
            l_keywordString = std::get<0>(*l_kwData);
        }
        return l_keywordString;
    };

    try
    {
        // Enable Reboot Guard
        if (constants::FAILURE == dbusUtility::EnableRebootGuard())
        {
            EventLogger::createAsyncPel(
                types::ErrorType::DbusFailure,
                types::SeverityType::Informational, __FILE__, __FUNCTION__, 0,
                std::string(
                    "Failed to enable BMC Reboot Guard while updating " +
                    l_keyWordIdentifier(i_paramsToWriteData)),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);

            return constants::FAILURE;
        }
#ifdef VPD_WRITE_SANITY_CHECK
        checkVpdWriteSanity(i_paramsToWriteData);
#endif
        std::shared_ptr<ParserInterface> l_vpdParserInstance =
            getVpdParserInstance();
        l_bytesUpdatedOnHardware =
            l_vpdParserInstance->writeKeywordOnHardware(i_paramsToWriteData);

#ifdef VPD_WRITE_SANITY_CHECK
        checkVpdWriteSanity(i_paramsToWriteData);
#endif
    }
    catch (const std::exception& l_exception)
    {
        types::ErrorType l_errorType;

        if (typeid(l_exception) == typeid(EccException))
        {
            l_errorType = types::ErrorType::EccCheckFailed;
        }
        else
        {
            l_errorType = types::ErrorType::InvalidVpdMessage;
        }

        EventLogger::createAsyncPel(
            l_errorType, types::SeverityType::Informational, __FILE__,
            __FUNCTION__, 0,
            "Error while updating keyword's value on hardware path [" +
                m_vpdFilePath + "], error: " + std::string(l_exception.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }

    // Disable Reboot Guard
    if (constants::FAILURE == dbusUtility::DisableRebootGuard())
    {
        EventLogger::createAsyncPel(
            types::ErrorType::DbusFailure, types::SeverityType::Critical,
            __FILE__, __FUNCTION__, 0,
            std::string("Failed to disable BMC Reboot Guard while updating " +
                        l_keyWordIdentifier(i_paramsToWriteData)),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }

    return l_bytesUpdatedOnHardware;
}

#ifdef VPD_WRITE_SANITY_CHECK
void Parser::checkVpdWriteSanity(
    const types::WriteVpdParams& i_paramsToWriteData) const noexcept
{
    try
    {
        // only ipz vpd data type is supported
        const types::IpzData* l_ipzData =
            std::get_if<types::IpzData>(&i_paramsToWriteData);
        if (!l_ipzData)
        {
            return;
        }

        // lambda to do ECC check on a specific record on a given FRU path.
        // returns the Keyword value on hardware
        auto l_checkRecordEccOnFru =
            [this, &l_ipzData = std::as_const(l_ipzData)](
                const std::string& i_fruPath) -> types::DbusVariantType {
            const auto& l_recordName = std::get<0>(*l_ipzData);
            const auto& l_keywordName = std::get<1>(*l_ipzData);

            std::shared_ptr<Parser> l_parserObj =
                std::make_shared<Parser>(i_fruPath, m_parsedJson);

            std::shared_ptr<ParserInterface> l_vpdParserInstance =
                l_parserObj->getVpdParserInstance();

            // check ECC of record on EEPROM
            if (!l_vpdParserInstance->recordEccCheck(*l_ipzData))
            {
                // Log a Predictive PEL including name of record
                EventLogger::createSyncPelWithInvCallOut(
                    types::ErrorType::VpdParseError,
                    types::SeverityType::Warning, __FILE__, __FUNCTION__,
                    constants::VALUE_0,
                    std::string(
                        "ECC check failed for record. Check user data for reason and record name. Re-program VPD."),
                    std::vector{std::make_tuple(i_fruPath,
                                                types::CalloutPriority::High)},
                    std::get<0>(*l_ipzData), i_fruPath, std::nullopt,
                    std::nullopt);

                // Dump Bad VPD to file
                vpdSpecificUtility::dumpBadVpd(i_fruPath,
                                               l_parserObj->getVpdVector());
            }

            return l_vpdParserInstance->readKeywordFromHardware(
                std::make_tuple(l_recordName, l_keywordName));
        };

        // check record ECC on primary EEPROM and get keyword value on hardware
        const auto l_primaryEepromKeywordValue =
            l_checkRecordEccOnFru(m_vpdFilePath);

        // check if there is any redundant EEPROM path
        const auto l_redundantFruPath =
            jsonUtility::getRedundantEepromPathFromJson(m_parsedJson,
                                                        m_vpdFilePath);

        if (!l_redundantFruPath.empty())
        {
            // check record ECC on redundant EEPROM and get keyword value on
            // hardware
            const auto l_redundantEepromKeywordValue =
                l_checkRecordEccOnFru(l_redundantFruPath);

            auto l_primaryValue =
                std::get_if<types::BinaryVector>(&l_primaryEepromKeywordValue);
            auto l_redundantValue = std::get_if<types::BinaryVector>(
                &l_redundantEepromKeywordValue);

            // Compare Record, Keyword on Primary and Redundant EEPROM
            //  and if not equal, log a PEL
            // log a predictive PEL
            if (l_primaryValue && l_redundantValue &&
                (*l_primaryValue != *l_redundantValue))
            {
                EventLogger::createSyncPelWithInvCallOut(
                    types::ErrorType::VpdParseError,
                    types::SeverityType::Warning, __FILE__, __FUNCTION__,
                    constants::VALUE_0,
                    std::string(
                        "Keyword value different on Primary and Redundant EEPROM. Check user data for details. Re-program VPD."),
                    std::vector{std::make_tuple(m_vpdFilePath,
                                                types::CalloutPriority::High)},
                    std::string(
                        "Record : " + std::get<0>(*l_ipzData) + " Keyword: " +
                        std::get<1>(*l_ipzData) + " Value on Primary EEPROM: " +
                        commonUtility::convertByteVectorToHex(*l_primaryValue) +
                        " Value on Redundant EEPROM: " +
                        commonUtility::convertByteVectorToHex(
                            *l_redundantValue)),
                    std::nullopt, std::nullopt, std::nullopt);
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to do VPD write sanity check on FRU :" + m_vpdFilePath +
            ". Error: " + std::string(l_ex.what()));
    }
}

#endif

} // namespace vpd
