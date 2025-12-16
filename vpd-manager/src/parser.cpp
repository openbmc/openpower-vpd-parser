#include "parser.hpp"

#include "constants.hpp"

#include <utility/dbus_utility.hpp>
#include <utility/event_logger_utility.hpp>
#include <utility/json_utility.hpp>
#include <utility/vpd_specific_utility.hpp>

#include <fstream>
#include <string>

namespace vpd
{
Parser::Parser(const std::string& vpdFilePath, nlohmann::json parsedJson,
               types::VpdCollectionMode i_vpdCollectionMode) :
    m_vpdFilePath(vpdFilePath), m_parsedJson(parsedJson),
    m_vpdCollectionMode(i_vpdCollectionMode),
    m_vpdModeBasedFruPath(vpdFilePath), m_logger(Logger::getLoggerInstance())
{
    std::error_code l_errCode;
    uint16_t l_errorCode = 0;

    commonUtility::getEffectiveFruPath(m_vpdCollectionMode,
                                       m_vpdModeBasedFruPath, l_errorCode);

    if (l_errorCode)
    {
        throw std::runtime_error(
            "Failed to get effective FRU path for : " + m_vpdFilePath);
    }

    // ToDo: Add minimum file size check in all the concert praser classes,
    // depends on their VPD type.
    if (!std::filesystem::exists(m_vpdModeBasedFruPath, l_errCode))
    {
        std::string l_message{"Parser object creation failed, file [" +
                              m_vpdModeBasedFruPath + "] doesn't exists."};

        if (l_errCode)
        {
            l_message += " Error message: " + l_errCode.message();
        }

        throw std::runtime_error(l_message);
    }

    // Read VPD offset if applicable.
    if (!m_parsedJson.empty())
    {
        m_vpdStartOffset =
            jsonUtility::getVPDOffset(m_parsedJson, vpdFilePath, l_errorCode);

        if (l_errorCode)
        {
            logging::logMessage(
                "Failed to get vpd offset for path [" + m_vpdFilePath +
                "], error: " + commonUtility::getErrCodeMsg(l_errorCode));
        }
    }
}

std::shared_ptr<vpd::ParserInterface> Parser::getVpdParserInstance()
{
    // Read the VPD data into a vector.
    uint16_t l_errCode = 0;
    vpdSpecificUtility::getVpdDataInVector(m_vpdModeBasedFruPath, m_vpdVector,
                                           m_vpdStartOffset, l_errCode);

    if (l_errCode)
    {
        logging::logMessage("Failed to get VPD in vector, error : " +
                            commonUtility::getErrCodeMsg(l_errCode));
    }

    // This will detect the type of parser required.
    std::shared_ptr<vpd::ParserInterface> l_parser = ParserFactory::getParser(
        m_vpdVector, m_vpdModeBasedFruPath, m_vpdStartOffset);

    return l_parser;
}

types::VPDMapVariant Parser::parse()
{
    std::shared_ptr<vpd::ParserInterface> l_parser = getVpdParserInstance();
    return l_parser->parse();
}

int Parser::updateVpdKeyword(const types::WriteVpdParams& i_paramsToWriteData,
                             types::DbusVariantType& o_updatedValue)
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
                m_vpdModeBasedFruPath +
                ", error: " + std::string(l_exception.what()));

            // TODO : Log PEL

            throw std::runtime_error(l_errMsg);
        }

        uint16_t l_errCode = 0;

        auto [l_fruPath, l_inventoryObjPath, l_redundantFruPath] =
            jsonUtility::getAllPathsToUpdateKeyword(m_parsedJson, m_vpdFilePath,
                                                    l_errCode);

        if (l_errCode == error_code::INVALID_INPUT_PARAMETER ||
            l_errCode == error_code::INVALID_JSON)
        {
            throw std::runtime_error(
                "Failed to get paths to update keyword. Error : " +
                commonUtility::getErrCodeMsg(l_errCode));
        }

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

                    // return the actual value updated on hardware
                    o_updatedValue = l_keywordValue;
                }
                catch (const std::exception& l_exception)
                {
                    // Unable to read keyword's value from hardware.
                    std::string l_errMsg(
                        "Error while reading keyword's value from hadware path " +
                        m_vpdModeBasedFruPath +
                        " , error: " + std::string(l_exception.what()));

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
            l_propertyName = vpdSpecificUtility::getDbusPropNameForGivenKw(
                l_propertyName, l_errCode);

            if (l_errCode)
            {
                logging::logMessage(
                    "Failed to get Dbus property name for given keyword, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }

            // Create D-bus object map
            types::ObjectMap l_dbusObjMap = {std::make_pair(
                l_inventoryObjPath,
                types::InterfaceMap{std::make_pair(
                    l_interfaceName, types::PropertyMap{std::make_pair(
                                         l_propertyName, l_keywordValue)})})};

            // Call method to update on dbus
            if (!dbusUtility::publishVpdOnDBus(std::move(l_dbusObjMap)))
            {
                // Update to dbus failed.
                std::string l_errMsg(
                    "publishVpdOnDBus is failed for object path: " +
                    l_inventoryObjPath);
                throw std::runtime_error(l_errMsg);
            }
        }

        if (l_errCode == error_code::ERROR_GETTING_REDUNDANT_PATH)
        {
            logging::logMessage(commonUtility::getErrCodeMsg(l_errCode));
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

int Parser::updateVpdKeyword(const types::WriteVpdParams& i_paramsToWriteData)
{
    types::DbusVariantType o_updatedValue;
    return updateVpdKeyword(i_paramsToWriteData, o_updatedValue);
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

        std::shared_ptr<ParserInterface> l_vpdParserInstance =
            getVpdParserInstance();
        l_bytesUpdatedOnHardware =
            l_vpdParserInstance->writeKeywordOnHardware(i_paramsToWriteData);
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
                m_vpdModeBasedFruPath +
                "], error: " + std::string(l_exception.what()),
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

} // namespace vpd
