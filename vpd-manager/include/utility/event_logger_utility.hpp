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
 * @param[in] exception - Exception object.
 *
 * @return - Valid ExceptionDataMap on success, otherwise map having
 * default value.
 */
inline types::ExceptionDataMap getExceptionData(const std::exception& exception)
{
    types::ExceptionDataMap errorInfo{
        {"ErrorType", types::ErrorType::UndefinedError},
        {"ErrorMsg", exception.what()}};

    try
    {
        if (typeid(exception) == typeid(DataException))
        {
            const DataException& ex =
                dynamic_cast<const DataException&>(exception);
            errorInfo["ErrorType"] = ex.getErrorType();
            errorInfo["ErrorMsg"] =
                std::string("Data Exception. Reason: ") + exception.what();
        }
        else if (typeid(exception) == typeid(EccException))
        {
            const EccException& ex =
                dynamic_cast<const EccException&>(exception);
            errorInfo["ErrorType"] = ex.getErrorType();
            errorInfo["ErrorMsg"] =
                std::string("Ecc Exception. Reason: ") + exception.what();
        }
        else if (typeid(exception) == typeid(JsonException))
        {
            const JsonException& ex =
                dynamic_cast<const JsonException&>(exception);
            errorInfo["ErrorType"] = ex.getErrorType();
            errorInfo["ErrorMsg"] =
                std::string("Json Exception. Reason: ") + exception.what();
        }
        else if (typeid(exception) == typeid(GpioException))
        {
            const GpioException& ex =
                dynamic_cast<const GpioException&>(exception);
            errorInfo["ErrorType"] = ex.getErrorType();
            errorInfo["ErrorMsg"] =
                std::string("Gpio Exception. Reason: ") + exception.what();
        }
        else if (typeid(exception) == typeid(DbusException))
        {
            const DbusException& ex =
                dynamic_cast<const DbusException&>(exception);
            errorInfo["ErrorType"] = ex.getErrorType();
            errorInfo["ErrorMsg"] =
                std::string("Dbus Exception. Reason: ") + exception.what();
        }
        else if (typeid(exception) == typeid(FirmwareException))
        {
            const FirmwareException& ex =
                dynamic_cast<const FirmwareException&>(exception);
            errorInfo["ErrorType"] = ex.getErrorType();
            errorInfo["ErrorMsg"] =
                std::string("Firmware Exception. Reason: ") + exception.what();
        }
        else if (typeid(exception) == typeid(EepromException))
        {
            const EepromException& ex =
                dynamic_cast<const EepromException&>(exception);
            errorInfo["ErrorType"] = ex.getErrorType();
            errorInfo["ErrorMsg"] =
                std::string("Eeprom Exception. Reason: ") + exception.what();
        }
        else if (typeid(exception) == typeid(std::runtime_error))
        {
            // Since it is a standard exception no casting is required and error
            // type is hardcoded.
            errorInfo["ErrorType"] = types::ErrorType::FirmwareError;
            errorInfo["ErrorMsg"] =
                std::string("Standard runtime exception. Reason: ") +
                exception.what();
        }
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to get error info, reason: " + std::string(ex.what()));
    }
    return errorInfo;
}

/**
 * @brief API to get Error type.
 *
 * @param[in] exception - Exception object.
 *
 * @return Error type set for the exception.
 * types::ErrorType::InternalFailure otherwise.
 */
inline types::ErrorType getErrorType(const std::exception& exception)
{
    const auto& exceptionDataMap = getExceptionData(exception);

    auto itrToErrType = exceptionDataMap.find("ErrorType");
    if (itrToErrType == exceptionDataMap.end())
    {
        return types::ErrorType::UndefinedError;
    }

    auto ptrToErrType = std::get_if<types::ErrorType>(&itrToErrType->second);
    if (!ptrToErrType)
    {
        return types::ErrorType::UndefinedError;
    }

    return *ptrToErrType;
}

/**
 * @brief API to get Error msg.
 *
 * @param[in] exception - Exception object.
 *
 * @return Error msg set for the specific exception. Default error msg
 * otherwise.
 */
inline std::string getErrorMsg(const std::exception& exception)
{
    const auto& exceptionDataMap = getExceptionData(exception);

    auto itrToErrMsg = exceptionDataMap.find("ErrorMsg");
    if (itrToErrMsg == exceptionDataMap.end())
    {
        return exception.what();
    }

    auto ptrToErrMsg = std::get_if<std::string>(&itrToErrMsg->second);
    if (!ptrToErrMsg)
    {
        return exception.what();
    }

    return *ptrToErrMsg;
}

/**
 * @brief API to get string representation of a Error type enum.
 *
 * @param[in] exception - Exception object.
 *
 * @return Error msg set for the specific error type. Default error msg
 * otherwise.
 */
inline std::string getErrorTypeString(
    const types::ErrorType& errorType) noexcept
{
    const auto entry = errorMsgMap.find(errorType);
    return (entry != errorMsgMap.end()
                ? entry->second
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
 * @param[in] asioConnection - Dbus Connection.
 * @param[in] errorType - Enum to map with event message name.
 * @param[in] severity - Severity of the event.
 * @param[in] callouts - Callout information, list of tuple having
 * inventory path and priority as input [optional].
 * @param[in] fileName - File name.
 * @param[in] funcName - Function name.
 * @param[in] internalRc - Internal return code.
 * @param[in] description - Error description.
 * @param[in] userData1 - Additional user data [optional].
 * @param[in] userData2 - Additional user data [optional].
 * @param[in] symFru - Symblolic FRU callout data [optional].
 * @param[in] procedure - Procedure callout data [optional].
 *
 * @throw exception in case of error.
 */
inline void createAsyncPelWithInventoryCallout(
    const std::shared_ptr<sdbusplus::asio::connection>& asioConnection,
    const types::ErrorType& errorType, const types::SeverityType& severity,
    const std::vector<types::InventoryCalloutData>& callouts,
    const std::string& fileName, const std::string& funcName,
    const uint8_t internalRc, const std::string& description,
    const std::optional<std::string> userData1,
    const std::optional<std::string> userData2,
    const std::optional<std::string> symFru,
    const std::optional<std::string> procedure)
{
    (void)symFru;
    (void)procedure;

    try
    {
        const std::string descriptionStr =
            (description.empty() ? "VPD generic error" : description);

        if (asioConnection == nullptr)
        {
            Logger::getLoggerInstance()->logMessage(std::format(
                "Error: Dbus Connection is empty, can't log async PEL. "
                "Error that couldn't log :{}",
                descriptionStr));
            return;
        }

        if (callouts.empty())
        {
            Logger::getLoggerInstance()->logMessage(
                "Callout information is missing to create PEL");
            // TODO: Revisit this instead of simply returning.
            return;
        }

        if (errorMsgMap.find(errorType) == errorMsgMap.end())
        {
            throw std::runtime_error(
                "Error type not found in the error message map to create PEL");
            // TODO: Need to handle, instead of throwing exception. Create
            // default message in message_registry.json.
        }

        const std::string& message = errorMsgMap.at(errorType);

        const std::string& severityStr =
            (severityMap.find(severity) != severityMap.end()
                 ? severityMap.at(severity)
                 : severityMap.at(types::SeverityType::Informational));

        std::string userData1Str = (userData1) ? (*userData1) : "";

        std::string userData2Str = (userData2) ? (*userData2) : "";

        const types::InventoryCalloutData& invCallout = callouts[0];
        // TODO: Need to handle multiple inventory path callout's, when multiple
        // callout's is supported by "Logging" service.

        const types::CalloutPriority& priorityEnum = get<1>(invCallout);

        const std::string& priority =
            (priorityMap.find(priorityEnum) != priorityMap.end()
                 ? priorityMap.at(priorityEnum)
                 : priorityMap.at(types::CalloutPriority::Low));

        const std::map<std::string, std::string> additionalData{
            {"FileName", fileName},
            {"FunctionName", funcName},
            {"DESCRIPTION", descriptionStr},
            {"CALLOUT_INVENTORY_PATH", get<0>(invCallout)},
            {"CALLOUT_PRIORITY", priority},
            {"InternalRc", std::to_string(internalRc)},
            {"UserData1", userData1Str},
            {"UserData2", userData2Str}};

        asioConnection->async_method_call(
            [](boost::system::error_code ec) {
                if (ec)
                {
                    Logger::getLoggerInstance()->logMessage(std::format(
                        "Async PEL with Inventory Callout failed. Reason : {}",
                        ec.message()));
                }
            },
            constants::eventLoggingServiceName,
            constants::eventLoggingObjectPath, constants::eventLoggingInterface,
            "Create", message, severityStr, additionalData);
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Create PEL failed with error: " + std::string(ex.what()));
    }
}

/**
 * @brief An API to create async PEL with device path callout.
 *
 * @param[in] asioConnection - Dbus Connection.
 * @param[in] errorType - Enum to map with event message name.
 * @param[in] severity - Severity of the event.
 * @param[in] callouts - Callout information, list of tuple having device
 * path and error number as input.
 * @param[in] fileName - File name.
 * @param[in] funcName - Function name.
 * @param[in] internalRc - Internal return code.
 * @param[in] description - Error description.
 * @param[in] userData1 - Additional user data [optional].
 * @param[in] userData2 - Additional user data [optional].
 */
inline void createAsyncPelWithDeviceCallout(
    const std::shared_ptr<sdbusplus::asio::connection>& asioConnection,
    const types::ErrorType errorType, const types::SeverityType severity,
    const std::vector<types::DeviceCalloutData>& callouts,
    const std::string& fileName, const std::string& funcName,
    const uint8_t internalRc, const std::string& description,
    const std::optional<std::string> userData1,
    const std::optional<std::string> userData2)
{
    try
    {
        const std::string descriptionStr =
            (description.empty() ? "VPD generic error" : description);

        if (asioConnection == nullptr)
        {
            throw std::runtime_error(
                "Error: Dbus Connection is empty, can't log async PEL.");
        }

        if (callouts.empty())
        {
            throw std::runtime_error(
                "Callout information is missing to create PEL.");
        }

        if (errorMsgMap.find(errorType) == errorMsgMap.end())
        {
            throw std::runtime_error(
                "Error type not found in the error message map to create PEL");
        }

        const std::string& message = errorMsgMap.at(errorType);

        const std::string& severityStr =
            (severityMap.find(severity) != severityMap.end()
                 ? severityMap.at(severity)
                 : severityMap.at(types::SeverityType::Informational));

        const std::string userData1Str = userData1.value_or("");
        const std::string userData2Str = userData2.value_or("");

        const types::DeviceCalloutData& devCallout = callouts[0];

        const std::map<std::string, std::string> additionalData{
            {"FileName", fileName},
            {"FunctionName", funcName},
            {"DESCRIPTION", descriptionStr},
            {"CALLOUT_DEVICE_PATH", std::get<0>(devCallout)},
            {"CALLOUT_ERRNO", std::get<1>(devCallout)},
            {"InternalRc", std::to_string(internalRc)},
            {"UserData1", userData1Str},
            {"UserData2", userData2Str}};

        asioConnection->async_method_call(
            [descriptionStr](boost::system::error_code ec) {
                if (ec)
                {
                    Logger::getLoggerInstance()->logMessage(std::format(
                        "Async PEL with Device Callout failed. Reason : {}. "
                        "Error that couldn't log: [{}]",
                        ec.message(), descriptionStr));
                }
            },
            constants::eventLoggingServiceName,
            constants::eventLoggingObjectPath, constants::eventLoggingInterface,
            "Create", message, severityStr, additionalData);
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Create PEL failed with error: {}. Error that couldn't log, "
            "FileName: [{}], FunctionName: [{}],"
            " DESCRIPTION: [{}], ERROR_TYPE: [{}]",
            std::string(ex.what()), fileName, funcName, description,
            std::to_underlying(errorType)));
    }
}

/**
 * @brief An API to create a PEL with I2c bus callout.
 *
 * @param[in] errorType - Enum to map with event message name.
 * @param[in] severity - Severity of the event.
 * @param[in] callouts - Callout information, list of tuple having i2c
 * bus, i2c address and error number as input.
 * @param[in] fileName - File name.
 * @param[in] funcName - Function name.
 * @param[in] internalRc - Internal return code.
 * @param[in] userData1 - Additional user data [optional].
 * @param[in] userData2 - Additional user data [optional].
 */
inline void createAsyncPelWithI2cBusCallout(
    const types::ErrorType errorType, const types::SeverityType severity,
    const std::vector<types::I2cBusCalloutData>& callouts,
    const std::string& fileName, const std::string& funcName,
    const uint8_t internalRc,
    const std::optional<std::pair<std::string, std::string>> userData1,
    const std::optional<std::pair<std::string, std::string>> userData2);

/**
 * @brief An API to create a PEL.
 *
 * @param[in] asioConnection - Dbus Connection.
 * @param[in] errorType - Enum to map with event message name.
 * @param[in] severity - Severity of the event.
 * @param[in] fileName - File name.
 * @param[in] funcName - Function name.
 * @param[in] internalRc - Internal return code.
 * @param[in] description - Error description.
 * @param[in] userData1 - Additional user data [optional].
 * @param[in] userData2 - Additional user data [optional].
 * @param[in] symFru - Symblolic FRU callout data [optional].
 * @param[in] procedure - Procedure callout data [optional].
 *
 * @todo: Symbolic FRU and procedure callout needs to be handled in this
 * API.
 */
inline void createAsyncPel(
    const std::shared_ptr<sdbusplus::asio::connection>& asioConnection,
    const types::ErrorType& errorType, const types::SeverityType& severity,
    const std::string& fileName, const std::string& funcName,
    const uint8_t internalRc, const std::string& description,
    const std::optional<std::string> userData1,
    const std::optional<std::string> userData2,
    const std::optional<std::string> symFru,
    const std::optional<std::string> procedure)
{
    (void)symFru;
    (void)procedure;
    try
    {
        const std::string descriptionStr =
            (description.empty() ? "VPD generic error" : description);

        if (asioConnection == nullptr)
        {
            Logger::getLoggerInstance()->logMessage(std::format(
                "Error: Dbus Connection is empty, can't log async PEL. "
                "Error that couldn't log :{}",
                descriptionStr));
            return;
        }

        if (errorMsgMap.find(errorType) == errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
            // TODO: Need to handle, instead of throwing an exception.
        }

        const std::string& message = errorMsgMap.at(errorType);

        const std::string& severityStr =
            (severityMap.find(severity) != severityMap.end()
                 ? severityMap.at(severity)
                 : severityMap.at(types::SeverityType::Informational));

        const std::string userData1Str = ((userData1) ? (*userData1) : "");

        const std::string userData2Str = ((userData2) ? (*userData2) : "");

        const std::map<std::string, std::string> additionalData{
            {"FileName", fileName},
            {"FunctionName", funcName},
            {"DESCRIPTION", descriptionStr},
            {"InternalRc", std::to_string(internalRc)},
            {"UserData1", userData1Str},
            {"UserData2", userData2Str}};

        asioConnection->async_method_call(
            [](boost::system::error_code ec) {
                if (ec)
                {
                    Logger::getLoggerInstance()->logMessage(std::format(
                        "Async PEL failed. Reason : {}", ec.message()));
                }
            },
            constants::eventLoggingServiceName,
            constants::eventLoggingObjectPath, constants::eventLoggingInterface,
            "Create", message, severityStr, additionalData);
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Async PEL creation failed with an error: " +
            std::string(ex.what()));
    }
}

/**
 * @brief An API to create PEL.
 *
 * This API makes synchronous call to phosphor-logging Create method.
 *
 * @param[in] errorType - Enum to map with event message name.
 * @param[in] severity - Severity of the event.
 * @param[in] fileName - File name.
 * @param[in] funcName - Function name.
 * @param[in] internalRc - Internal return code.
 * @param[in] description - Error description.
 * @param[in] userData1 - Additional user data [optional].
 * @param[in] userData2 - Additional user data [optional].
 * @param[in] symFru - Symblolic FRU callout data [optional].s
 * @param[in] procedure - Procedure callout data [optional].
 *
 * @todo: Symbolic FRU and procedure callout needs to be handled in this
 * API.
 */
inline void createSyncPel(
    const types::ErrorType& errorType, const types::SeverityType& severity,
    const std::string& fileName, const std::string& funcName,
    const uint8_t internalRc, const std::string& description,
    const std::optional<std::string> userData1,
    const std::optional<std::string> userData2,
    const std::optional<std::string> symFru,
    const std::optional<std::string> procedure)
{
    (void)symFru;
    (void)procedure;
    try
    {
        if (errorMsgMap.find(errorType) == errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
            // TODO: Need to handle, instead of throwing an exception.
        }

        const std::string& message = errorMsgMap.at(errorType);

        const std::string& severityStr =
            (severityMap.find(severity) != severityMap.end()
                 ? severityMap.at(severity)
                 : severityMap.at(types::SeverityType::Informational));

        const std::string descriptionStr =
            ((!description.empty() ? description : "VPD generic error"));

        const std::string userData1Str = ((userData1) ? (*userData1) : "");

        const std::string userData2Str = ((userData2) ? (*userData2) : "");

        std::map<std::string, std::string> additionalData{
            {"FileName", fileName},
            {"FunctionName", funcName},
            {"DESCRIPTION", descriptionStr},
            {"InternalRc", std::to_string(internalRc)},
            {"UserData1", userData1Str},
            {"UserData2", userData2Str}};

        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call(constants::eventLoggingServiceName,
                                constants::eventLoggingObjectPath,
                                constants::eventLoggingInterface, "Create");
        method.append(message, severityStr, additionalData);
        bus.call(method);
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Sync PEL creation failed with an error: " +
            std::string(ex.what()));
    }
}

/**
 * @brief An API to create a synchronous PEL with inventory path callout.
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
 * @param[in] errorType - Enum to map with event message name.
 * @param[in] severity - Severity of the event.
 * @param[in] fileName - File name.
 * @param[in] funcName - Function name.
 * @param[in] internalRc - Internal return code.
 * @param[in] description - Error description.
 * @param[in] callouts - Callout information.
 * @param[in] userData1 - Additional user data [optional].
 * @param[in] userData2 - Additional user data [optional].
 * @param[in] symFru - Symblolic FRU callout data [optional].
 * @param[in] procedure - Procedure callout data [optional].
 *
 */
inline void createSyncPelWithInvCallOut(
    const types::ErrorType& errorType, const types::SeverityType& severity,
    const std::string& fileName, const std::string& funcName,
    const uint8_t internalRc, const std::string& description,
    const std::vector<types::InventoryCalloutData>& callouts,
    const std::optional<std::string> userData1,
    const std::optional<std::string> userData2,
    [[maybe_unused]] const std::optional<std::string> symFru,
    [[maybe_unused]] const std::optional<std::string> procedure)
{
    try
    {
        if (callouts.empty())
        {
            createSyncPel(errorType, severity, fileName, funcName, internalRc,
                          description, userData1, userData2, symFru, procedure);
            Logger::getLoggerInstance()->logMessage(
                "Callout list is empty, creating PEL without call out");
            return;
        }

        if (errorMsgMap.find(errorType) == errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
        }

        // Path to hold callout inventory path.
        std::string calloutInvPath;

        uint16_t errCode = 0;

        // check if callout path is a valid inventory path. if not, get the JSON
        // object to get inventory path.
        if (std::get<0>(callouts[0])
                .compare(constants::VALUE_0, strlen(constants::pimPath),
                         constants::pimPath) != constants::STR_CMP_SUCCESS)
        {
            std::error_code ec;
            // implies json dependent execution.
            if (std::filesystem::exists(INVENTORY_JSON_SYM_LINK, ec))
            {
                if (!ec)
                {
                    calloutInvPath = jsonUtility::getInventoryObjPathFromJson(
                        std::get<0>(callouts[0]), errCode);
                }
                else
                {
                    Logger::getLoggerInstance()->logMessage(
                        "Error finding symlink. Continue with given path");
                }
            }
        }

        if (calloutInvPath.empty())
        {
            calloutInvPath = std::get<0>(callouts[0]);

            if (errCode)
            {
                Logger::getLoggerInstance()->logMessage(
                    "Failed to get inventory object path from JSON for FRU [" +
                    std::get<0>(callouts[0]) +
                    "], error : " + commonUtility::getErrCodeMsg(errCode));
            }
        }

        const std::map<std::string, std::string> additionalData{
            {"FileName", fileName},
            {"FunctionName", funcName},
            {"DESCRIPTION",
             (!description.empty() ? description : "VPD generic error")},
            {"CALLOUT_INVENTORY_PATH", calloutInvPath},
            {"InternalRc", std::to_string(internalRc)},
            {"UserData1", userData1 ? *userData1 : ""},
            {"UserData2", userData2 ? *userData2 : ""}};

        const std::string& severityStr =
            (severityMap.find(severity) != severityMap.end()
                 ? severityMap.at(severity)
                 : severityMap.at(types::SeverityType::Informational));

        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call(constants::eventLoggingServiceName,
                                constants::eventLoggingObjectPath,
                                constants::eventLoggingInterface, "Create");
        method.append(errorMsgMap.at(errorType), severityStr, additionalData);
        bus.call(method);
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Sync PEL creation with inventory path failed with error: " +
            std::string(ex.what()));
    }
}

/**
 * @brief An API to create a synchronous PEL with device path callout.
 *
 * This API calls phosphor-logging method to create PEL, and also handles
 * device path callout.
 * If device path is not provided in the callout, it will create
 * PEL without call out. Currently only one callout is handled in this API.
 *
 * @param[in] errorType - Enum to map with event message name.
 * @param[in] severity - Severity of the event.
 * @param[in] callouts - Callout information.
 * @param[in] fileName - File name.
 * @param[in] funcName - Function name.
 * @param[in] internalRc - Internal return code.
 * @param[in] description - Error description.
 * @param[in] userData1 - Additional user data [optional].
 * @param[in] userData2 - Additional user data [optional].
 */
inline void createSyncPelWithDeviceCallout(
    const types::ErrorType& errorType, const types::SeverityType& severity,
    const std::vector<types::DeviceCalloutData>& callouts,
    const std::string& fileName, const std::string& funcName,
    const uint8_t internalRc, const std::string& description,
    const std::optional<std::string> userData1,
    const std::optional<std::string> userData2) noexcept
{
    try
    {
        if (callouts.empty())
        {
            createSyncPel(errorType, severity, fileName, funcName, internalRc,
                          description, userData1, userData2, std::nullopt,
                          std::nullopt);
            Logger::getLoggerInstance()->logMessage(std::format(
                "Callout list is empty, creating PEL without call out. "
                "FileName: {}, FunctionName: {}",
                fileName, funcName));
            return;
        }

        if (errorMsgMap.find(errorType) == errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
        }

        const types::DeviceCalloutData& devCallout = callouts[0];

        const std::map<std::string, std::string> additionalData{
            {"FileName", fileName},
            {"FunctionName", funcName},
            {"DESCRIPTION",
             (!description.empty() ? description : "VPD generic error")},
            {"CALLOUT_DEVICE_PATH", std::get<0>(devCallout)},
            {"CALLOUT_ERRNO", std::get<1>(devCallout)},
            {"InternalRc", std::to_string(internalRc)},
            {"UserData1", userData1.value_or("")},
            {"UserData2", userData2.value_or("")}};

        const std::string& severityStr =
            (severityMap.find(severity) != severityMap.end()
                 ? severityMap.at(severity)
                 : severityMap.at(types::SeverityType::Informational));

        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call(constants::eventLoggingServiceName,
                                constants::eventLoggingObjectPath,
                                constants::eventLoggingInterface, "Create");
        method.append(errorMsgMap.at(errorType), severityStr, additionalData);
        bus.call(method);
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Sync PEL creation with device path failed with error: {}. "
            "Error that couldn't log,"
            " FileName: [{}], FunctionName: [{}],"
            " Description: [{}], ErrorType: [{}]",
            ex.what(), fileName, funcName, description,
            std::to_underlying(errorType)));
    }
}
} // namespace EventLogger
} // namespace vpd
