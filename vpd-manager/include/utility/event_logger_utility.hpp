#pragma once

#include "config.h"

#include "common_utility.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include "json_utility.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <systemd/sd-bus.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

namespace vpd
{
namespace EventLogger
{
const std::unordered_map<types::SeverityType, std::string> severityMap = {
    {types::SeverityType::Notice,
     "xyz.openbmc_project.Logging.Entry.Level.Notice"},
    {types::SeverityType::Informational,
     "xyz.openbmc_project.Logging.Entry.Level.Informational"},
    {types::SeverityType::Debug,
     "xyz.openbmc_project.Logging.Entry.Level.Debug"},
    {types::SeverityType::Warning,
     "xyz.openbmc_project.Logging.Entry.Level.Warning"},
    {types::SeverityType::Critical,
     "xyz.openbmc_project.Logging.Entry.Level.Critical"},
    {types::SeverityType::Emergency,
     "xyz.openbmc_project.Logging.Entry.Level.Emergency"},
    {types::SeverityType::Alert,
     "xyz.openbmc_project.Logging.Entry.Level.Alert"},
    {types::SeverityType::Error,
     "xyz.openbmc_project.Logging.Entry.Level.Error"}};

const std::unordered_map<types::ErrorType, std::string> errorMsgMap = {
    {types::ErrorType::DefaultValue, "com.ibm.VPD.Error.DefaultValue"},
    {types::ErrorType::UndefinedError, "com.ibm.VPD.Error.UndefinedError"},
    {types::ErrorType::InvalidVpdMessage, "com.ibm.VPD.Error.InvalidVPD"},
    {types::ErrorType::VpdMismatch, "com.ibm.VPD.Error.Mismatch"},
    {types::ErrorType::InvalidEeprom, "com.ibm.VPD.Error.InvalidEepromPath"},
    {types::ErrorType::EccCheckFailed, "com.ibm.VPD.Error.EccCheckFailed"},
    {types::ErrorType::JsonFailure, "com.ibm.VPD.Error.InvalidJson"},
    {types::ErrorType::DbusFailure, "com.ibm.VPD.Error.DbusFailure"},
    {types::ErrorType::InvalidSystem, "com.ibm.VPD.Error.UnknownSystemType"},
    {types::ErrorType::EssentialFru, "com.ibm.VPD.Error.RequiredFRUMissing"},
    {types::ErrorType::GpioError, "com.ibm.VPD.Error.GPIOError"},
    {types::ErrorType::InternalFailure,
     "xyz.openbmc_project.Common.Error.InternalFailure"},
    {types::ErrorType::FruMissing, "com.ibm.VPD.Error.RequiredFRUMissing"},
    {types::ErrorType::SystemTypeMismatch,
     "com.ibm.VPD.Error.SystemTypeMismatch"},
    {types::ErrorType::UnknownSystemSettings,
     "com.ibm.VPD.Error.UnknownSystemSettings"},
    {types::ErrorType::FirmwareError, "com.ibm.VPD.Error.FirmwareError"},
    {types::ErrorType::VpdParseError, "com.ibm.VPD.Error.VPDParseError"}};

const std::unordered_map<types::CalloutPriority, std::string> priorityMap = {
    {types::CalloutPriority::High, "H"},
    {types::CalloutPriority::Medium, "M"},
    {types::CalloutPriority::MediumGroupA, "A"},
    {types::CalloutPriority::MediumGroupB, "B"},
    {types::CalloutPriority::MediumGroupC, "C"},
    {types::CalloutPriority::Low, "L"}};

/**
 * @brief API to get error info based on the exception.
 *
 * @param[in] i_exception - Exception object.
 *
 * @return - Valid ExceptionDataMap on success, otherwise map having
 * default value.
 */
inline types::ExceptionDataMap getExceptionData(
    const std::exception& i_exception)
{
    types::ExceptionDataMap l_errorInfo{
        {"ErrorType", types::ErrorType::UndefinedError},
        {"ErrorMsg", i_exception.what()}};

    try
    {
        if (typeid(i_exception) == typeid(DataException))
        {
            const DataException& l_ex =
                dynamic_cast<const DataException&>(i_exception);
            l_errorInfo["ErrorType"] = l_ex.getErrorType();
            l_errorInfo["ErrorMsg"] =
                std::string("Data Exception. Reason: ") + i_exception.what();
        }
        else if (typeid(i_exception) == typeid(EccException))
        {
            const EccException& l_ex =
                dynamic_cast<const EccException&>(i_exception);
            l_errorInfo["ErrorType"] = l_ex.getErrorType();
            l_errorInfo["ErrorMsg"] =
                std::string("Ecc Exception. Reason: ") + i_exception.what();
        }
        else if (typeid(i_exception) == typeid(JsonException))
        {
            const JsonException& l_ex =
                dynamic_cast<const JsonException&>(i_exception);
            l_errorInfo["ErrorType"] = l_ex.getErrorType();
            l_errorInfo["ErrorMsg"] =
                std::string("Json Exception. Reason: ") + i_exception.what();
        }
        else if (typeid(i_exception) == typeid(GpioException))
        {
            const GpioException& l_ex =
                dynamic_cast<const GpioException&>(i_exception);
            l_errorInfo["ErrorType"] = l_ex.getErrorType();
            l_errorInfo["ErrorMsg"] =
                std::string("Gpio Exception. Reason: ") + i_exception.what();
        }
        else if (typeid(i_exception) == typeid(DbusException))
        {
            const DbusException& l_ex =
                dynamic_cast<const DbusException&>(i_exception);
            l_errorInfo["ErrorType"] = l_ex.getErrorType();
            l_errorInfo["ErrorMsg"] =
                std::string("Dbus Exception. Reason: ") + i_exception.what();
        }
        else if (typeid(i_exception) == typeid(FirmwareException))
        {
            const FirmwareException& l_ex =
                dynamic_cast<const FirmwareException&>(i_exception);
            l_errorInfo["ErrorType"] = l_ex.getErrorType();
            l_errorInfo["ErrorMsg"] =
                std::string("Firmware Exception. Reason: ") +
                i_exception.what();
        }
        else if (typeid(i_exception) == typeid(EepromException))
        {
            const EepromException& l_ex =
                dynamic_cast<const EepromException&>(i_exception);
            l_errorInfo["ErrorType"] = l_ex.getErrorType();
            l_errorInfo["ErrorMsg"] =
                std::string("Eeprom Exception. Reason: ") + i_exception.what();
        }
        else if (typeid(i_exception) == typeid(std::runtime_error))
        {
            // Since it is a standard exception no casting is required and error
            // type is hardcoded.
            l_errorInfo["ErrorType"] = types::ErrorType::FirmwareError;
            l_errorInfo["ErrorMsg"] =
                std::string("Standard runtime exception. Reason: ") +
                i_exception.what();
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to get error info, reason: " + std::string(l_ex.what()));
    }
    return l_errorInfo;
}

/**
 * @brief API to get Error type.
 *
 * @param[in] i_exception - Exception object.
 *
 * @return Error type set for the exception.
 * types::ErrorType::InternalFailure otherwise.
 */
inline types::ErrorType getErrorType(const std::exception& i_exception)
{
    const auto& l_exceptionDataMap = getExceptionData(i_exception);

    auto l_itrToErrType = l_exceptionDataMap.find("ErrorType");
    if (l_itrToErrType == l_exceptionDataMap.end())
    {
        return types::ErrorType::UndefinedError;
    }

    auto l_ptrToErrType =
        std::get_if<types::ErrorType>(&l_itrToErrType->second);
    if (!l_ptrToErrType)
    {
        return types::ErrorType::UndefinedError;
    }

    return *l_ptrToErrType;
}

/**
 * @brief API to get Error msg.
 *
 * @param[in] i_exception - Exception object.
 *
 * @return Error msg set for the specific exception. Default error msg
 * otherwise.
 */
inline std::string getErrorMsg(const std::exception& i_exception)
{
    const auto& l_exceptionDataMap = getExceptionData(i_exception);

    auto l_itrToErrMsg = l_exceptionDataMap.find("ErrorMsg");
    if (l_itrToErrMsg == l_exceptionDataMap.end())
    {
        return i_exception.what();
    }

    auto l_ptrToErrMsg = std::get_if<std::string>(&l_itrToErrMsg->second);
    if (!l_ptrToErrMsg)
    {
        return i_exception.what();
    }

    return *l_ptrToErrMsg;
}

/**
 * @brief API to get string representation of a Error type enum.
 *
 * @param[in] i_exception - Exception object.
 *
 * @return Error msg set for the specific error type. Default error msg
 * otherwise.
 */
inline std::string getErrorTypeString(
    const types::ErrorType& i_errorType) noexcept
{
    const auto l_entry = errorMsgMap.find(i_errorType);
    return (l_entry != errorMsgMap.end()
                ? l_entry->second
                : errorMsgMap.at(types::ErrorType::UndefinedError));
}

/**
 * @brief An API to create a PEL with inventory path callout.
 *
 * This API calls an async method to create PEL, and also handles inventory
 * path callout.
 *
 * Note: If inventory path callout info is not provided, it will create a
 * PEL without any callout. Currently only one callout is handled in this
 * API.
 *
 * @todo: Symbolic FRU and procedure callout needs to be handled in this
 * API.
 *
 * @param[in] i_errorType - Enum to map with event message name.
 * @param[in] i_severity - Severity of the event.
 * @param[in] i_callouts - Callout information, list of tuple having
 * inventory path and priority as input [optional].
 * @param[in] i_fileName - File name.
 * @param[in] i_funcName - Function name.
 * @param[in] i_internalRc - Internal return code.
 * @param[in] i_description - Error description.
 * @param[in] i_userData1 - Additional user data [optional].
 * @param[in] i_userData2 - Additional user data [optional].
 * @param[in] i_symFru - Symblolic FRU callout data [optional].
 * @param[in] i_procedure - Procedure callout data [optional].
 *
 * @throw exception in case of error.
 */
inline void createAsyncPelWithInventoryCallout(
    const types::ErrorType& i_errorType, const types::SeverityType& i_severity,
    const std::vector<types::InventoryCalloutData>& i_callouts,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc, const std::string& i_description,
    const std::optional<std::string> i_userData1,
    const std::optional<std::string> i_userData2,
    const std::optional<std::string> i_symFru,
    const std::optional<std::string> i_procedure)
{
    (void)i_symFru;
    (void)i_procedure;

    try
    {
        if (i_callouts.empty())
        {
            logging::logMessage("Callout information is missing to create PEL");
            // TODO: Revisit this instead of simpley returning.
            return;
        }

        if (errorMsgMap.find(i_errorType) == errorMsgMap.end())
        {
            throw std::runtime_error(
                "Error type not found in the error message map to create PEL");
            // TODO: Need to handle, instead of throwing exception. Create
            // default message in message_registry.json.
        }

        const std::string& l_message = errorMsgMap.at(i_errorType);

        const std::string& l_severity =
            (severityMap.find(i_severity) != severityMap.end()
                 ? severityMap.at(i_severity)
                 : severityMap.at(types::SeverityType::Informational));

        std::string l_description =
            (!i_description.empty() ? i_description : "VPD generic error");

        std::string l_userData1 = (i_userData1) ? (*i_userData1) : "";

        std::string l_userData2 = (i_userData2) ? (*i_userData2) : "";

        const types::InventoryCalloutData& l_invCallout = i_callouts[0];
        // TODO: Need to handle multiple inventory path callout's, when multiple
        // callout's is supported by "Logging" service.

        const types::CalloutPriority& l_priorityEnum = get<1>(l_invCallout);

        const std::string& l_priority =
            (priorityMap.find(l_priorityEnum) != priorityMap.end()
                 ? priorityMap.at(l_priorityEnum)
                 : priorityMap.at(types::CalloutPriority::Low));

        sd_bus* l_sdBus = nullptr;
        sd_bus_default(&l_sdBus);

        const uint8_t l_additionalDataCount = 8;
        auto l_rc = sd_bus_call_method_async(
            l_sdBus, NULL, constants::eventLoggingServiceName,
            constants::eventLoggingObjectPath, constants::eventLoggingInterface,
            "Create", NULL, NULL, "ssa{ss}", l_message.c_str(),
            l_severity.c_str(), l_additionalDataCount, "FileName",
            i_fileName.c_str(), "FunctionName", i_funcName.c_str(),
            "InternalRc", std::to_string(i_internalRc).c_str(), "DESCRIPTION",
            l_description.c_str(), "UserData1", l_userData1.c_str(),
            "UserData2", l_userData2.c_str(), "CALLOUT_INVENTORY_PATH",
            get<0>(l_invCallout).c_str(), "CALLOUT_PRIORITY",
            l_priority.c_str());

        if (l_rc < 0)
        {
            logging::logMessage(
                "Error calling sd_bus_call_method_async, Message = " +
                std::string(strerror(-l_rc)));
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Create PEL failed with error: " + std::string(l_ex.what()));
    }
}

/**
 * @brief An API to create a PEL with device path callout.
 *
 * @param[in] i_errorType - Enum to map with event message name.
 * @param[in] i_severity - Severity of the event.
 * @param[in] i_callouts - Callout information, list of tuple having device
 * path and error number as input.
 * @param[in] i_fileName - File name.
 * @param[in] i_funcName - Function name.
 * @param[in] i_internalRc - Internal return code.
 * @param[in] i_userData1 - Additional user data [optional].
 * @param[in] i_userData2 - Additional user data [optional].
 */
inline void createAsyncPelWithI2cDeviceCallout(
    const types::ErrorType i_errorType, const types::SeverityType i_severity,
    const std::vector<types::DeviceCalloutData>& i_callouts,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc,
    const std::optional<std::pair<std::string, std::string>> i_userData1,
    const std::optional<std::pair<std::string, std::string>> i_userData2);

/**
 * @brief An API to create a PEL with I2c bus callout.
 *
 * @param[in] i_errorType - Enum to map with event message name.
 * @param[in] i_severity - Severity of the event.
 * @param[in] i_callouts - Callout information, list of tuple having i2c
 * bus, i2c address and error number as input.
 * @param[in] i_fileName - File name.
 * @param[in] i_funcName - Function name.
 * @param[in] i_internalRc - Internal return code.
 * @param[in] i_userData1 - Additional user data [optional].
 * @param[in] i_userData2 - Additional user data [optional].
 */
inline void createAsyncPelWithI2cBusCallout(
    const types::ErrorType i_errorType, const types::SeverityType i_severity,
    const std::vector<types::I2cBusCalloutData>& i_callouts,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc,
    const std::optional<std::pair<std::string, std::string>> i_userData1,
    const std::optional<std::pair<std::string, std::string>> i_userData2);

/**
 * @brief An API to create a PEL.
 *
 * @param[in] i_errorType - Enum to map with event message name.
 * @param[in] i_severity - Severity of the event.
 * @param[in] i_fileName - File name.
 * @param[in] i_funcName - Function name.
 * @param[in] i_internalRc - Internal return code.
 * @param[in] i_description - Error description.
 * @param[in] i_userData1 - Additional user data [optional].
 * @param[in] i_userData2 - Additional user data [optional].
 * @param[in] i_symFru - Symblolic FRU callout data [optional].
 * @param[in] i_procedure - Procedure callout data [optional].
 *
 * @todo: Symbolic FRU and procedure callout needs to be handled in this
 * API.
 */
inline void createAsyncPel(
    const types::ErrorType& i_errorType, const types::SeverityType& i_severity,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc, const std::string& i_description,
    const std::optional<std::string> i_userData1,
    const std::optional<std::string> i_userData2,
    const std::optional<std::string> i_symFru,
    const std::optional<std::string> i_procedure)
{
    (void)i_symFru;
    (void)i_procedure;
    try
    {
        if (errorMsgMap.find(i_errorType) == errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
            // TODO: Need to handle, instead of throwing an exception.
        }

        const std::string& l_message = errorMsgMap.at(i_errorType);

        const std::string& l_severity =
            (severityMap.find(i_severity) != severityMap.end()
                 ? severityMap.at(i_severity)
                 : severityMap.at(types::SeverityType::Informational));

        const std::string l_description =
            ((!i_description.empty() ? i_description : "VPD generic error"));

        const std::string l_userData1 = ((i_userData1) ? (*i_userData1) : "");

        const std::string l_userData2 = ((i_userData2) ? (*i_userData2) : "");

        sd_bus* l_sdBus = nullptr;
        sd_bus_default(&l_sdBus);

        // VALUE_6 represents the additional data pair count passing to create
        // PEL. If there any change in additional data, we need to pass the
        // correct number.
        auto l_rc = sd_bus_call_method_async(
            l_sdBus, NULL, constants::eventLoggingServiceName,
            constants::eventLoggingObjectPath, constants::eventLoggingInterface,
            "Create", NULL, NULL, "ssa{ss}", l_message.c_str(),
            l_severity.c_str(), constants::VALUE_6, "FileName",
            i_fileName.c_str(), "FunctionName", i_funcName.c_str(),
            "InternalRc", std::to_string(i_internalRc).c_str(), "DESCRIPTION",
            l_description.c_str(), "UserData1", l_userData1.c_str(),
            "UserData2", l_userData2.c_str());

        if (l_rc < 0)
        {
            logging::logMessage(
                "Error calling sd_bus_call_method_async, Message = " +
                std::string(strerror(-l_rc)));
        }
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage("Async PEL creation failed with an error: " +
                            std::string(l_ex.what()));
    }
}

/**
 * @brief An API to create PEL.
 *
 * This API makes synchronous call to phosphor-logging Create method.
 *
 * @param[in] i_errorType - Enum to map with event message name.
 * @param[in] i_severity - Severity of the event.
 * @param[in] i_fileName - File name.
 * @param[in] i_funcName - Function name.
 * @param[in] i_internalRc - Internal return code.
 * @param[in] i_description - Error description.
 * @param[in] i_userData1 - Additional user data [optional].
 * @param[in] i_userData2 - Additional user data [optional].
 * @param[in] i_symFru - Symblolic FRU callout data [optional].s
 * @param[in] i_procedure - Procedure callout data [optional].
 *
 * @todo: Symbolic FRU and procedure callout needs to be handled in this
 * API.
 */
inline void createSyncPel(
    const types::ErrorType& i_errorType, const types::SeverityType& i_severity,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc, const std::string& i_description,
    const std::optional<std::string> i_userData1,
    const std::optional<std::string> i_userData2,
    const std::optional<std::string> i_symFru,
    const std::optional<std::string> i_procedure)
{
    (void)i_symFru;
    (void)i_procedure;
    try
    {
        if (errorMsgMap.find(i_errorType) == errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
            // TODO: Need to handle, instead of throwing an exception.
        }

        const std::string& l_message = errorMsgMap.at(i_errorType);

        const std::string& l_severity =
            (severityMap.find(i_severity) != severityMap.end()
                 ? severityMap.at(i_severity)
                 : severityMap.at(types::SeverityType::Informational));

        const std::string l_description =
            ((!i_description.empty() ? i_description : "VPD generic error"));

        const std::string l_userData1 = ((i_userData1) ? (*i_userData1) : "");

        const std::string l_userData2 = ((i_userData2) ? (*i_userData2) : "");

        std::map<std::string, std::string> l_additionalData{
            {"FileName", i_fileName},
            {"FunctionName", i_funcName},
            {"DESCRIPTION", l_description},
            {"InteranlRc", std::to_string(i_internalRc)},
            {"UserData1", l_userData1.c_str()},
            {"UserData2", l_userData2.c_str()}};

        auto l_bus = sdbusplus::bus::new_default();
        auto l_method =
            l_bus.new_method_call(constants::eventLoggingServiceName,
                                  constants::eventLoggingObjectPath,
                                  constants::eventLoggingInterface, "Create");
        l_method.append(l_message, l_severity, l_additionalData);
        l_bus.call(l_method);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage("Sync PEL creation failed with an error: " +
                            std::string(l_ex.what()));
    }
}

/**
 * @brief An API to create a synchronus PEL with inventory path callout.
 *
 * This API calls phosphor-logging method to create PEL, and also handles
 * inventory path callout. In case called with EEPROM path, will look for
 * JSON at symbolic link and if present will fetch inventory path for that
 * EEPROM.
 *
 * Note: In case of any error in fetching JSON or converting the EEPROM
 * path, the API will log PEL with path passed in callout parameter.
 * If inventory/EEPROM path is not provided in the callout, it will create
 * PEL without call out. Currently only one callout is handled in this API.
 *
 * @todo: Symbolic FRU and procedure callout needs to be handled in this
 * API.
 *
 * @param[in] i_errorType - Enum to map with event message name.
 * @param[in] i_severity - Severity of the event.
 * @param[in] i_fileName - File name.
 * @param[in] i_funcName - Function name.
 * @param[in] i_internalRc - Internal return code.
 * @param[in] i_description - Error description.
 * @param[in] i_callouts - Callout information.
 * @param[in] i_userData1 - Additional user data [optional].
 * @param[in] i_userData2 - Additional user data [optional].
 * @param[in] i_symFru - Symblolic FRU callout data [optional].
 * @param[in] i_procedure - Procedure callout data [optional].
 *
 */
inline void createSyncPelWithInvCallOut(
    const types::ErrorType& i_errorType, const types::SeverityType& i_severity,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc, const std::string& i_description,
    const std::vector<types::InventoryCalloutData>& i_callouts,
    const std::optional<std::string> i_userData1,
    const std::optional<std::string> i_userData2,
    [[maybe_unused]] const std::optional<std::string> i_symFru,
    [[maybe_unused]] const std::optional<std::string> i_procedure)
{
    try
    {
        if (i_callouts.empty())
        {
            createSyncPel(i_errorType, i_severity, i_fileName, i_funcName,
                          i_internalRc, i_description, i_userData1, i_userData2,
                          i_symFru, i_procedure);
            logging::logMessage(
                "Callout list is empty, creating PEL without call out");
            return;
        }

        if (errorMsgMap.find(i_errorType) == errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
        }

        // Path to hold callout inventory path.
        std::string l_calloutInvPath;

        uint16_t l_errCode = 0;

        // check if callout path is a valid inventory path. if not, get the JSON
        // object to get inventory path.
        if (std::get<0>(i_callouts[0])
                .compare(constants::VALUE_0, strlen(constants::pimPath),
                         constants::pimPath) != constants::STR_CMP_SUCCESS)
        {
            std::error_code l_ec;
            // implies json dependent execution.
            if (std::filesystem::exists(INVENTORY_JSON_SYM_LINK, l_ec))
            {
                if (!l_ec)
                {
                    nlohmann::json l_parsedJson = jsonUtility::getParsedJson(
                        INVENTORY_JSON_SYM_LINK, l_errCode);

                    if (l_errCode)
                    {
                        logging::logMessage(
                            "Failed to parse JSON file [ " +
                            std::string(INVENTORY_JSON_SYM_LINK) +
                            " ], error : " +
                            commonUtility::getErrCodeMsg(l_errCode));
                    }

                    l_calloutInvPath = jsonUtility::getInventoryObjPathFromJson(
                        l_parsedJson, std::get<0>(i_callouts[0]), l_errCode);
                }
                else
                {
                    logging::logMessage(
                        "Error finding symlink. Continue with given path");
                }
            }
        }

        if (l_calloutInvPath.empty())
        {
            l_calloutInvPath = std::get<0>(i_callouts[0]);

            if (l_errCode)
            {
                logging::logMessage(
                    "Failed to get inventory object path from JSON for FRU [" +
                    std::get<0>(i_callouts[0]) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }
        }

        const std::map<std::string, std::string> l_additionalData{
            {"FileName", i_fileName},
            {"FunctionName", i_funcName},
            {"DESCRIPTION",
             (!i_description.empty() ? i_description : "VPD generic error")},
            {"CALLOUT_INVENTORY_PATH", l_calloutInvPath},
            {"InteranlRc", std::to_string(i_internalRc)},
            {"UserData1", ((i_userData1) ? (*i_userData1) : "").c_str()},
            {"UserData2", ((i_userData2) ? (*i_userData2) : "").c_str()}};

        const std::string& l_severity =
            (severityMap.find(i_severity) != severityMap.end()
                 ? severityMap.at(i_severity)
                 : severityMap.at(types::SeverityType::Informational));

        auto l_bus = sdbusplus::bus::new_default();
        auto l_method =
            l_bus.new_method_call(constants::eventLoggingServiceName,
                                  constants::eventLoggingObjectPath,
                                  constants::eventLoggingInterface, "Create");
        l_method.append(errorMsgMap.at(i_errorType), l_severity,
                        l_additionalData);
        l_bus.call(l_method);
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Sync PEL creation with inventory path failed with error: " +
            std::string(l_ex.what()));
    }
}
} // namespace EventLogger
} // namespace vpd
