#pragma once

#include "config_manager.hpp"
#include "error_codes.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <gpiod.hpp>
#include <nlohmann/json.hpp>
#include <utility/common_utility.hpp>

#include <expected>
#include <fstream>
#include <type_traits>
#include <unordered_map>

namespace vpd
{
namespace jsonUtility
{

// forward declaration of API for function map.
bool processSystemCmdTag(const std::string& vpdFilePath,
                         const std::string& baseAction,
                         const std::string& flagToProcess, uint16_t& errCode);

// forward declaration of API for function map.
bool processGpioPresenceTag(
    const std::string& vpdFilePath, const std::string& baseAction,
    const std::string& flagToProcess, uint16_t& errCode);

// forward declaration of API for function map.
bool processSetGpioTag(const std::string& vpdFilePath,
                       const std::string& baseAction,
                       const std::string& flagToProcess, uint16_t& errCode);

// Function pointers to process tags from config JSON.
using FunctionPtr =
    bool (*)(const std::string& vpdFilePath, const std::string& baseAction,
             const std::string& flagToProcess, uint16_t& errCode);

inline std::unordered_map<std::string, FunctionPtr> functionMap{
    {"gpioPresence", processGpioPresenceTag},
    {"setGpio", processSetGpioTag},
    {"systemCmd", processSystemCmdTag}};

/**
 * @brief API to read VPD offset from JSON file.
 *
 * @param[in] vpdFilePath - VPD file path.
 * @param[out] errCode - To set error code in case of error.
 * @return VPD offset if found in JSON, 0 otherwise.
 */
inline size_t getVPDOffset(const std::string& vpdFilePath, uint16_t& errCode)
{
    errCode = 0;
    if (vpdFilePath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return 0;
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return 0;
    }

    const auto configJsonResult = configManager->getJsonObj(vpdFilePath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return 0;
    }

    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    if (sysCfgJsonObj["frus"].contains(vpdFilePath))
    {
        return sysCfgJsonObj["frus"][vpdFilePath].at(0).value("offset", 0);
    }

    const nlohmann::json& fruList =
        sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& fru : fruList.items())
    {
        const auto fruPath = fru.key();

        // check if given path is redundant FRU path
        if (vpdFilePath ==
            sysCfgJsonObj["frus"][fruPath].at(0).value("redundantEeprom", ""))
        {
            // Return the offset of redundant EEPROM taken from JSON.
            return sysCfgJsonObj["frus"][fruPath].at(0).value("offset", 0);
        }
    }

    return 0;
}

/**
 * @brief API to parse respective JSON.
 *
 * @param[in] pathToJson - Path to JSON.
 * @param[out] errCode - To set error code in case of error.
 * @return on success parsed JSON. On failure empty JSON object.
 *
 * Note: Caller has to handle it in case an empty JSON object is received.
 */
inline nlohmann::json getParsedJson(const std::string& pathToJson,
                                    uint16_t& errCode) noexcept
{
    errCode = 0;
    if (pathToJson.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return nlohmann::json{};
    }

    if (!std::filesystem::exists(pathToJson))
    {
        errCode = error_code::FILE_NOT_FOUND;
        return nlohmann::json{};
    }

    if (std::filesystem::is_empty(pathToJson))
    {
        errCode = error_code::EMPTY_FILE;
        return nlohmann::json{};
    }

    std::ifstream jsonFile(pathToJson);
    if (!jsonFile)
    {
        errCode = error_code::FILE_ACCESS_ERROR;
        return nlohmann::json{};
    }

    try
    {
        return nlohmann::json::parse(jsonFile);
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::JSON_PARSE_ERROR;
        return nlohmann::json{};
    }
}

/**
 * @brief Get inventory object path from system config JSON.
 *
 * Given either D-bus inventory path/FRU EEPROM path/redundant EEPROM path,
 * this API returns D-bus inventory path if present in JSON.
 *
 * @param[in] vpdPath - Path to where VPD is stored.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success a valid path is returned, on failure an empty string is
 * returned.
 *
 * Note: Caller has to handle it in case an empty string is received.
 */
inline std::string getInventoryObjPathFromJson(const std::string& vpdPath,
                                               uint16_t& errCode) noexcept
{
    errCode = 0;
    if (vpdPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return std::string{};
    }

    const auto configJsonResult = configManager->getJsonObj(vpdPath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return std::string{};
    }

    const auto& sysCfgJsonObj = configJsonResult.value().get();

    // check if given path is FRU path
    if (sysCfgJsonObj["frus"].contains(vpdPath))
    {
        return sysCfgJsonObj["frus"][vpdPath].at(0).value("inventoryPath", "");
    }

    const nlohmann::json& fruList =
        sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& fru : fruList.items())
    {
        const auto fruPath = fru.key();
        const auto invObjPath =
            sysCfgJsonObj["frus"][fruPath].at(0).value("inventoryPath", "");

        // check if given path is redundant FRU path or inventory path
        if (vpdPath == sysCfgJsonObj["frus"][fruPath].at(0).value(
                           "redundantEeprom", "") ||
            (vpdPath == invObjPath))
        {
            return invObjPath;
        }
    }

    return std::string{};
}

/**
 * @brief Process "PostFailAction" defined in config JSON.
 *
 * In case there is some error in the processing of "preAction" execution and a
 * set of procedure needs to be done as a part of post fail action. This base
 * action can be defined in the config JSON for that FRU and it will be handled
 * under this API.
 *
 * @param[in] vpdFilePath - EEPROM file path
 * @param[in] flagToProcess - To identify which flag(s) needs to be processed
 * under PostFailAction tag of config JSON.
 * @param[out] errCode - To set error code in case of error
 * @return - success or failure
 */
inline bool executePostFailAction(const std::string& vpdFilePath,
                                  const std::string& flagToProcess,
                                  uint16_t& errCode)
{
    errCode = 0;
    if (vpdFilePath.empty() || flagToProcess.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return false;
    }

    const auto configJsonResult = configManager->getJsonObj(vpdFilePath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return false;
    }

    const nlohmann::json& parsedConfigJson = configJsonResult.value().get();

    if (!parsedConfigJson["frus"].contains(vpdFilePath))
    {
        errCode = error_code::FRU_PATH_NOT_FOUND;
        return false;
    }

    if (!parsedConfigJson["frus"][vpdFilePath].at(0).contains("postFailAction"))
    {
        errCode = error_code::MISSING_ACTION_TAG;
        return false;
    }

    if (!(parsedConfigJson["frus"][vpdFilePath].at(0))["postFailAction"]
             .contains(flagToProcess))
    {
        errCode = error_code::MISSING_FLAG;
        return false;
    }

    for (const auto& tags : (parsedConfigJson["frus"][vpdFilePath].at(
             0))["postFailAction"][flagToProcess]
                                .items())
    {
        auto itrToFunction = functionMap.find(tags.key());
        if (itrToFunction != functionMap.end())
        {
            if (!itrToFunction->second(vpdFilePath, "postFailAction",
                                       flagToProcess, errCode))
            {
                if (errCode)
                {
                    Logger::getLoggerInstance()->logMessage(
                        tags.key() + " failed for [" + vpdFilePath +
                        "]. Reason " + commonUtility::getErrCodeMsg(errCode));
                }
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Process "systemCmd" tag for a given FRU.
 *
 * The API will process "systemCmd" tag if it is defined in the config
 * JSON for the given FRU.
 *
 * @param[in] parsedConfigJson - config JSON
 * @param[in] vpdFilePath - EEPROM file path
 * @param[in] baseAction - Base action for which this tag has been called.
 * @param[in] flagToProcess - Flag nested under the base action for which this
 * tag has been called.
 * @param[out] errCode - To set error code in case of error.
 * @return Execution status.
 */
inline bool processSystemCmdTag(
    const std::string& vpdFilePath, const std::string& baseAction,
    const std::string& flagToProcess, uint16_t& errCode)
{
    errCode = 0;
    if (vpdFilePath.empty() || baseAction.empty() || flagToProcess.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return false;
    }

    const auto configJsonResult = configManager->getJsonObj(vpdFilePath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return false;
    }

    const auto& parsedConfigJson = configJsonResult.value().get();

    try
    {
        if (!((parsedConfigJson["frus"][vpdFilePath].at(
                   0)[baseAction][flagToProcess]["systemCmd"])
                  .contains("cmd")))
        {
            errCode = error_code::MISSING_FLAG;
            return false;
        }

        const std::string& systemCommand =
            parsedConfigJson["frus"][vpdFilePath].at(
                0)[baseAction][flagToProcess]["systemCmd"]["cmd"];

        commonUtility::executeCmd(systemCommand, errCode);
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::ERROR_PROCESSING_SYSTEM_CMD;
        return false;
    }
    return true;
}

/**
 * @brief Checks for presence of a given FRU using GPIO line.
 *
 * This API returns the presence information of the FRU corresponding to the
 * given VPD file path by setting the presence pin.
 *
 * @param[in] parsedConfigJson - config JSON
 * @param[in] vpdFilePath - EEPROM file path
 * @param[in] baseAction - Base action for which this tag has been called.
 * @param[in] flagToProcess - Flag nested under the base action for which this
 * tag has been called.
 * @param[out] errCode - To set error code in case of error
 * @return Execution status.
 */
inline bool processGpioPresenceTag(
    const std::string& vpdFilePath, const std::string& baseAction,
    const std::string& flagToProcess, uint16_t& errCode)
{
    errCode = 0;
    std::string presencePinName;
    try
    {
        if (vpdFilePath.empty() || baseAction.empty() || flagToProcess.empty())
        {
            errCode = error_code::INVALID_INPUT_PARAMETER;
            return false;
        }

        auto configManager = ConfigManager::getInstance();
        if (!configManager)
        {
            errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
            return false;
        }

        const auto configJsonResult = configManager->getJsonObj(vpdFilePath);
        if (!configJsonResult.has_value())
        {
            errCode = configJsonResult.error();
            return false;
        }

        const auto& parsedConfigJson = configJsonResult.value().get();

        if (!(((parsedConfigJson["frus"][vpdFilePath].at(
                    0)[baseAction][flagToProcess]["gpioPresence"])
                   .contains("pin")) &&
              ((parsedConfigJson["frus"][vpdFilePath].at(
                    0)[baseAction][flagToProcess]["gpioPresence"])
                   .contains("value"))))
        {
            errCode = error_code::JSON_MISSING_GPIO_INFO;
            return false;
        }

        // get the pin name
        presencePinName = parsedConfigJson["frus"][vpdFilePath].at(
            0)[baseAction][flagToProcess]["gpioPresence"]["pin"];

        // get the pin value
        uint8_t presencePinValue = parsedConfigJson["frus"][vpdFilePath].at(
            0)[baseAction][flagToProcess]["gpioPresence"]["value"];

        gpiod::line presenceLine = gpiod::find_line(presencePinName);

        if (!presenceLine)
        {
            errCode = error_code::GPIO_LINE_EXCEPTION;
            throw GpioException("Couldn't find the GPIO line.");
        }

        presenceLine.request({"Read the presence line",
                              gpiod::line_request::DIRECTION_INPUT, 0});

        if (presencePinValue != presenceLine.get_value())
        {
            // As false is being returned in this case, caller needs to know
            // that it is not due to some exception. It is because the pin was
            // read correctly but was not having expected value.
            errCode = error_code::DEVICE_NOT_PRESENT;
            return false;
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        // In catch without error code implies some standard exception.
        if (!errCode)
        {
            errCode = error_code::STANDARD_EXCEPTION;
        }

        std::string errMsg = "Exception on GPIO line: ";
        errMsg += presencePinName;
        errMsg += " Reason: ";
        errMsg += ex.what();
        errMsg += " File: " + vpdFilePath + " Pel Logged";

        // ToDo -- Check if PEL is required. Update Internal Rc code.
        /**
         uint16_t errCode = 0;
         EventLogger::createAsyncPelWithInventoryCallout(
            EventLogger::getErrorType(ex), types::SeverityType::Informational,
            {{getInventoryObjPathFromJson(vpdFilePath, errCode),
              types::CalloutPriority::High}},
            std::source_location::current().file_name(),
            std::source_location::current().function_name(), 0, errMsg,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
         */

        Logger::getLoggerInstance()->logMessage(errMsg);

        // Except when GPIO pin value is false, we go and try collecting the
        // FRU VPD as we couldn't able to read GPIO pin value due to some
        // error/exception. So returning true in error scenario.
        return true;
    }
}

/**
 * @brief Process "setGpio" tag for a given FRU.
 *
 * This API enables the GPIO line.
 *
 * @param[in] vpdFilePath - EEPROM file path
 * @param[in] baseAction - Base action for which this tag has been called.
 * @param[in] flagToProcess - Flag nested under the base action for which this
 * tag has been called.
 * @param[out] errCode - To set error code in case of error
 * @return Execution status.
 */
inline bool processSetGpioTag(
    const std::string& vpdFilePath, const std::string& baseAction,
    const std::string& flagToProcess, uint16_t& errCode)
{
    errCode = 0;
    std::string pinName;
    try
    {
        if (vpdFilePath.empty() || baseAction.empty() || flagToProcess.empty())
        {
            errCode = error_code::INVALID_INPUT_PARAMETER;
            return false;
        }

        auto configManager = ConfigManager::getInstance();
        if (!configManager)
        {
            errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
            return false;
        }

        const auto configJsonResult = configManager->getJsonObj(vpdFilePath);
        if (!configJsonResult.has_value())
        {
            errCode = configJsonResult.error();
            return false;
        }

        const auto& parsedConfigJson = configJsonResult.value().get();

        if (!(((parsedConfigJson["frus"][vpdFilePath].at(
                    0)[baseAction][flagToProcess]["setGpio"])
                   .contains("pin")) &&
              ((parsedConfigJson["frus"][vpdFilePath].at(
                    0)[baseAction][flagToProcess]["setGpio"])
                   .contains("value"))))
        {
            errCode = error_code::JSON_MISSING_GPIO_INFO;
            return false;
        }

        pinName = parsedConfigJson["frus"][vpdFilePath].at(
            0)[baseAction][flagToProcess]["setGpio"]["pin"];

        // Get the value to set
        uint8_t pinValue = parsedConfigJson["frus"][vpdFilePath].at(
            0)[baseAction][flagToProcess]["setGpio"]["value"];

        Logger::getLoggerInstance()->logMessage(
            "Setting GPIO: " + pinName + " to " + std::to_string(pinValue));

        gpiod::line outputLine = gpiod::find_line(pinName);

        if (!outputLine)
        {
            errCode = error_code::GPIO_LINE_EXCEPTION;
            throw GpioException("Couldn't find GPIO line.");
        }

        outputLine.request(
            {"FRU Action", ::gpiod::line_request::DIRECTION_OUTPUT, 0},
            pinValue);
        return true;
    }
    catch (const std::exception& ex)
    {
        std::string errMsg = "Exception on GPIO line: ";
        errMsg += pinName;
        errMsg += " Reason: ";
        errMsg += ex.what();
        errMsg += " File: " + vpdFilePath + " Pel Logged";

        // ToDo -- Update Internal RC code
        /**
         uint16_t errCode = 0;
         EventLogger::createAsyncPelWithInventoryCallout(
            EventLogger::getErrorType(ex), types::SeverityType::Informational,
            {{getInventoryObjPathFromJson(vpdFilePath, errCode),
              types::CalloutPriority::High}},
            std::source_location::current().file_name(),
            std::source_location::current().function_name(), 0, errMsg,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
         */

        Logger::getLoggerInstance()->logMessage(errMsg);

        return false;
    }
}

/**
 * @brief Anonymous namespace to abstract helper APIs only required by other
 * json utility methods.
 */
namespace
{
/**
 * @brief Validate input parameters for base action execution.
 *
 * This function is private to this file and used only by executeBaseAction.
 *
 * @param[in] parsedConfigJson - Config JSON object
 * @param[in] action - Base action to be performed
 * @param[in] fruPath - EEPROM file path
 * @param[in] flagToProcess - Flag to identify which tags need processing
 * @param[out] errCode - Error code set in case of validation errors
 *
 * @return true if validation passes, false otherwise
 */
inline bool validateBaseActionInputs(
    const nlohmann::json& parsedConfigJson, const std::string& action,
    const std::string& fruPath, const std::string& flagToProcess,
    uint16_t& errCode)
{
    errCode = 0;

    if (flagToProcess.empty() || action.empty() || fruPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    if (!parsedConfigJson["frus"].contains(fruPath))
    {
        errCode = error_code::FRU_PATH_NOT_FOUND;
        return false;
    }

    if (!parsedConfigJson["frus"][fruPath].at(0).contains(action))
    {
        errCode = error_code::MISSING_ACTION_TAG;
        return false;
    }

    if (!(parsedConfigJson["frus"][fruPath].at(0))[action].contains(
            flagToProcess))
    {
        errCode = error_code::MISSING_FLAG;
        return false;
    }

    return true;
}

/**
 * @brief Handle non-GPIO presence tag processing.
 *
 * @param[in] tagName - Name of the tag being processed
 * @param[in] tagErrorCode - Error code from tag execution
 * @param[in,out] result - Result structure to update
 */
inline void updateNonPresenceTagOutput(const std::string& tagName,
                                       const uint16_t tagErrorCode,
                                       types::BaseActionResult& result) noexcept
{
    result.m_success = false;
    result.m_failedTag = tagName;
    result.m_failedTagErrorCode = tagErrorCode;
}

/**
 * @brief API to update presence tag execution output.
 *
 * @param[in] tagProcessingRes - Whether the tag execution succeeded
 * @param[in] tagErrorCode - Error code from tag execution
 * @param[in,out] result - Result structure to update
 *
 * @return true to continue processing rest of the tags, false to stop.
 */
inline bool updatePresenceTagOutput(const bool tagProcessingRes,
                                    const uint16_t tagErrorCode,
                                    types::BaseActionResult& result) noexcept
{
    result.m_gpioPresenceErrorCode = tagErrorCode;

    if (!tagProcessingRes)
    {
        if (tagErrorCode == error_code::DEVICE_NOT_PRESENT)
        {
            // PIN was read successfully and device is found to be absent.
            //  Stop processing other tags
            result.m_presenceStatus = types::PresenceStatus::ABSENT;
            return false;
        }

        // Some other error. Continue processing other tags
        result.m_presenceStatus = types::PresenceStatus::UNKNOWN;

        // This API will be called via two paths.
        // i) Some other tag failed and presence is being called explicitly. In
        // that case the condition will fail. So the failed tag will be filled
        // by the tag which actually failed.
        // ii) IF presence was called in the flow and was the first tag to fail
        // then this condition will hold and the failed tag should be explicity
        // updated with gpiopresence.
        if (result.m_failedTag.empty())
        {
            result.m_success = false;
            result.m_failedTag = "gpioPresence";
            result.m_failedTagErrorCode = tagErrorCode;
        }
    }
    else
    {
        // We also return true from processGpioPresenceTag in case of any
        // exception. So that we go and process other tags and try for
        // collection of FRU. If error code returned is returned along side true
        // it indicates an exception. Set presence status as unknown and still
        // return true for other tag processing. Now presence will be decided
        // based on collection status.
        if (tagErrorCode != 0)
        {
            result.m_presenceStatus = types::PresenceStatus::UNKNOWN;
        }
        else
        {
            // PIN was read successfully and device is found to be present.
            result.m_presenceStatus = types::PresenceStatus::PRESENT;
        }
    }
    return true;
}
} // namespace

/**
 * @brief Process base action with error reporting and presence detection.
 *
 * This is an enhanced version of executeBaseAction that provides detailed
 * information about tag execution failures and GPIO presence detection results.
 *
 * If any FRU(s) requires special handling, this base action can be defined for
 * that FRU in the config JSON. This function processes all tags under the
 * specified action and flag, with special handling for gpioPresence tag.
 *
 * Key behaviors:
 * 1. If a non-gpioPresence tag fails:
 *    - Records the failure information
 *    - Checks if gpioPresence is defined in the same action/flag
 *    - If defined and not yet processed, executes gpioPresence
 *    - Returns detailed result with both failures if gpioPresence also fails
 *
 * 2. If gpioPresence tag fails:
 *    - If error is DEVICE_NOT_PRESENT: Sets presenceStatus=ABSENT (success
 * case)
 *    - If error is PIN read error/exception: Sets presenceStatus=UNKNOWN,
 *      continues processing other tags
 *    - Records gpioPresence error information
 *
 * 3. If all tags succeed:
 *    - Returns success with appropriate presence status if gpioPresence was
 * processed
 *
 * @param[in] action - Base action to be performed (e.g., "preAction",
 * "postAction")
 * @param[in] fruPath - EEPROM file path
 * @param[in] flagToProcess - Flag to identify which tags need processing
 * @param[out] errCode - Error code set in case of validation errors
 *
 * @return types::BaseActionResult structure containing:
 *         - success: Overall execution status
 *         - failedTag: Name of the tag that failed (empty if all succeeded)
 *         - failedTagErrorCode: Error code from the failed tag
 *         - presenceStatus: Device presence status
 * (NOT_APPLICABLE/PRESENT/ABSENT/UNKNOWN)
 *         - gpioPresenceErrorCode: Error code from gpioPresence execution
 */
inline types::BaseActionResult executeBaseAction(
    const std::string& action, const std::string& fruPath,
    const std::string& flagToProcess, uint16_t& errCode)
{
    types::BaseActionResult actionRes;

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        actionRes.m_success = false;
        return actionRes;
    }

    const auto configJsonResult = configManager->getJsonObj(fruPath);

    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        actionRes.m_success = false;
        return actionRes;
    }

    const nlohmann::json& parsedConfigJson = configJsonResult.value().get();

    if (!validateBaseActionInputs(parsedConfigJson, action, fruPath,
                                  flagToProcess, errCode))
    {
        actionRes.m_success = false;
        return actionRes;
    }

    const nlohmann::json& tagsJson =
        (parsedConfigJson["frus"][fruPath].at(0))[action][flagToProcess];

    // Flag to track if gpio presence has been processed.
    bool gpioPresenceProcessed = false;

    for (const auto& tag : tagsJson.items())
    {
        const std::string& tagName = tag.key();
        auto itrToFunction = functionMap.find(tagName);

        // If there is no code support for the tag. Do nothing.
        if (itrToFunction == functionMap.end())
        {
            Logger::getLoggerInstance()->logMessage(
                std::format("Tag: {} is not supported by the code.", tagName));
            continue;
        }

        errCode = 0;
        bool tagProcessingRes =
            itrToFunction->second(fruPath, action, flagToProcess, errCode);

        if (tagName == "gpioPresence")
        {
            gpioPresenceProcessed = true;
            if (!updatePresenceTagOutput(tagProcessingRes, errCode, actionRes))
            {
                break;
            }
        }
        else
        {
            // Non-gpioPresence tag processed successfully. Move to another.
            if (tagProcessingRes)
            {
                continue;
            }

            // Tag failed - record the failure
            updateNonPresenceTagOutput(tagName, errCode, actionRes);

            // Execute gpioPresence if defined and not yet processed
            if (tagsJson.contains("gpioPresence") && !gpioPresenceProcessed)
            {
                uint16_t gpioErrorCode = 0;

                // Process GPIO presence pin explicitly.
                bool gpioRes = processGpioPresenceTag(
                    fruPath, action, flagToProcess, gpioErrorCode);

                updatePresenceTagOutput(gpioRes, gpioErrorCode, actionRes);
            }
            break;
        }
    }
    return actionRes;
}

/**
 * @brief API to check if input FRU path is a redundant EEPROM
 *
 * Checks if the input FRU path is a redundant EEPROM by checking
 * whether it has a "isRedundant" flag or appears as the value of a
 * "redundantEeprom" tag.
 *
 * @param[in] vpdPath - Path to where VPD is stored.
 *
 * @return std::expected<bool, error_code>
 *         - true: if the input path is a redundant EEPROM.
 *         - false: if the input path is a primary FRU EEPROM.
 *         - relevant error code in case of failure.
 */
inline std::expected<bool, uint16_t> isRedundantEeprom(
    const std::string& vpdPath) noexcept
{
    if (vpdPath.empty())
    {
        return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        return std::unexpected(error_code::CONFIG_MANAGER_UNINITIALIZED);
    }

    const auto sysCfgJsonObjResult = configManager->getJsonObj(vpdPath);

    if (!sysCfgJsonObjResult.has_value())
    {
        return std::unexpected(sysCfgJsonObjResult.error());
    }

    const auto& sysCfgJsonObj = sysCfgJsonObjResult.value().get();

    if (sysCfgJsonObj["frus"].contains(vpdPath))
    {
        return sysCfgJsonObj["frus"][vpdPath].at(0).value("isRedundant", false);
    }

    const nlohmann::json& fruList =
        sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& fru : fruList.items())
    {
        const std::string& fruPath = fru.key();
        const std::string& redundantFruPath =
            sysCfgJsonObj["frus"][fruPath].at(0).value("redundantEeprom", "");

        if (redundantFruPath == vpdPath)
        {
            return true;
        }
    }

    return std::unexpected(error_code::FRU_PATH_NOT_FOUND);
}

/**
 * @brief Get redundant FRU path from system config JSON
 *
 * Given either D-bus inventory path/FRU path/redundant FRU path, this
 * API returns the redundant FRU path taken from "redundantEeprom" tag from
 * system config JSON. For a D-Bus inventory path, a redundant
 * FRU path is returned only if the inventory path corresponds to a base FRU.
 *
 * @param[in] vpdPath - Path to where VPD is stored.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success return valid path, on failure return empty string.
 */
inline std::string getRedundantEepromPathFromJson(const std::string& vpdPath,
                                                  uint16_t& errCode)
{
    errCode = 0;
    if (vpdPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return std::string{};
    }

    const auto sysCfgJsonObjResult = configManager->getJsonObj(vpdPath);

    if (!sysCfgJsonObjResult.has_value())
    {
        errCode = sysCfgJsonObjResult.error();
        return std::string{};
    }

    // For a redundant EEPROM path, the JSON lookup succeeds because
    // redundant EEPROM paths are present as JSON keys, and returns an
    // empty string. If the input is already a redundant EEPROM path,
    // return the input path directly.
    const auto isRedundantEepromPath = jsonUtility::isRedundantEeprom(vpdPath);
    if (!isRedundantEepromPath.has_value())
    {
        // isRedundantEeprom() sets error, FRU_PATH_NOT_FOUND when the input is
        // an inventory path. Do not return early if the input is an inventory
        // path.
        if (isRedundantEepromPath.error() != error_code::FRU_PATH_NOT_FOUND)
        {
            Logger::getLoggerInstance()->logMessage(
                "Failed to check if path [" + vpdPath +
                "] is a redundant EEPROM, error : " +
                commonUtility::getErrCodeMsg(isRedundantEepromPath.error()));

            errCode = isRedundantEepromPath.error();
            return std::string{};
        }
    }
    else if (isRedundantEepromPath.value())
    {
        return vpdPath;
    }

    const auto& sysCfgJsonObj = sysCfgJsonObjResult.value().get();

    // check if given path is FRU path
    if (sysCfgJsonObj["frus"].contains(vpdPath))
    {
        return sysCfgJsonObj["frus"][vpdPath].at(0).value("redundantEeprom",
                                                          "");
    }

    const nlohmann::json& fruList =
        sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& fru : fruList.items())
    {
        const std::string& fruPath = fru.key();
        const std::string& redundantFruPath =
            sysCfgJsonObj["frus"][fruPath].at(0).value("redundantEeprom", "");
        const auto isRedundantFru =
            sysCfgJsonObj["frus"][fruPath].at(0).value("isRedundant", false);

        // check if given path is inventory path.
        if ((sysCfgJsonObj["frus"][fruPath].at(0).value("inventoryPath", "") ==
             vpdPath))
        {
            if (isRedundantFru)
            {
                return fruPath;
            }
            return redundantFruPath;
        }
    }

    return std::string();
}

/**
 * @brief Get all hardware paths from system config JSON
 *
 * Given an inventory path, the API returns all the hardware paths, if the
 * inventory path is a base FRU. If it is not a base FRU, the hardware paths are
 * returned if it inherits records from the EEPROM through 'inherit',
 * 'copyRecords' or 'skipRecords' keywords. Otherwise, an empty list is
 * returned.
 *
 * @param[in] inventoryPath - inventory path
 *
 * @return std::expected<types::EepromPathList, error_code>
 *         - list of hardware paths, if the inventory path inherits from an
 *           EEPROM.
 *         - empty list, if the inventory path is present in the JSON but does
 *           not inherit from any EEPROM.
 *         - relevant error code in case of failure.
 */
inline std::expected<types::EepromPathList, error_code>
    getAllHardwarePathFromJson(const std::string& inventoryPath) noexcept
{
    try
    {
        if (inventoryPath.empty())
        {
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        auto configManager = ConfigManager::getInstance();
        if (!configManager)
        {
            return std::unexpected(error_code::CONFIG_MANAGER_UNINITIALIZED);
        }

        const auto configJsonResult = configManager->getJsonObj(inventoryPath);
        if (!configJsonResult.has_value())
        {
            return std::unexpected(configJsonResult.error());
        }

        const auto& parsedConfigJson = configJsonResult.value().get();

        bool isInvPathFound = false;

        const nlohmann::json& fruList =
            parsedConfigJson["frus"].get_ref<const nlohmann::json::object_t&>();

        for (const auto& fru : fruList.items())
        {
            const std::string& fruPath = fru.key();
            const auto& entries = fru.value();

            const auto it = std::ranges::find_if(
                entries, [&inventoryPath](const nlohmann::json& entry) {
                    return entry.value("inventoryPath", "") == inventoryPath;
                });

            if (it == entries.end())
            {
                continue;
            }

            isInvPathFound = true;

            if (it->value("isRedundant", false))
            {
                continue;
            }

            if (!it->value("inherit", true) && !it->contains("copyRecords") &&
                !it->contains("skipRecords"))
            {
                continue;
            }

            types::EepromPathList eepromList{fruPath};

            const std::string redundantEepromPath =
                entries.at(0).value("redundantEeprom", "");
            if (!redundantEepromPath.empty())
            {
                eepromList.emplace_back(redundantEepromPath);
            }

            return eepromList;
        }

        if (!isInvPathFound)
        {
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Exception [{}] while getting hardware paths for {}: {}",
            typeid(ex).name(), inventoryPath, ex.what()));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }

    return types::EepromPathList{};
}

/**
 * @brief Get FRU EEPROM path from system config JSON
 *
 * Given either D-bus inventory path/FRU EEPROM path/redundant EEPROM path,
 * this API returns FRU EEPROM path if present in JSON.
 *
 * @param[in] vpdPath - Path to where VPD is stored.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success return valid path, on failure return empty string.
 */
inline std::string getFruPathFromJson(const std::string& vpdPath,
                                      uint16_t& errCode)
{
    errCode = 0;
    if (vpdPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return std::string{};
    }

    const auto configJsonResult = configManager->getJsonObj(vpdPath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return std::string{};
    }

    const auto& parsedConfigJson = configJsonResult.value().get();

    // check if given path is FRU path
    if (parsedConfigJson["frus"].contains(vpdPath))
    {
        const auto isRedundantEepromPath =
            jsonUtility::isRedundantEeprom(vpdPath);
        if (!isRedundantEepromPath.has_value())
        {
            errCode = ERROR_CHECKING_REDUNDANT_PATH;
            return std::string{};
        }
        if (!isRedundantEepromPath.value())
        {
            return vpdPath;
        }
    }

    const nlohmann::json& fruList =
        parsedConfigJson["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& fru : fruList.items())
    {
        const auto fruPath = fru.key();

        // check if given path is redundant FRU path or inventory path
        if ((vpdPath == parsedConfigJson["frus"][fruPath].at(0).value(
                            "redundantEeprom", "") ||
             vpdPath == parsedConfigJson["frus"][fruPath].at(0).value(
                            "inventoryPath", "")) &&
            !parsedConfigJson["frus"][fruPath].at(0).value("isRedundant",
                                                           false))
        {
            return fruPath;
        }
    }

    errCode = error_code::FRU_PATH_NOT_FOUND;
    return std::string();
}

/**
 * @brief An API to check backup and restore VPD is required.
 *
 * The API checks if there is provision for backup and restore mentioned in the
 * system config JSON, by looking "backupRestoreConfigPath" tag.
 * Checks if the path mentioned is a hardware path, by checking if the file path
 * exists and size of contents in the path.
 *
 * @param[out] errCode - To set error code in case of error.
 *
 * @return true if backup and restore is required, false otherwise.
 */
inline bool isBackupAndRestoreRequired(uint16_t& errCode)
{
    errCode = 0;

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return false;
    }

    // get the full system configuration JSON
    const auto configJsonResult = configManager->getJsonObj();
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return false;
    }

    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    const std::string& backupAndRestoreCfgFilePath =
        sysCfgJsonObj.value("backupRestoreConfigPath", "");

    if (!backupAndRestoreCfgFilePath.empty() &&
        std::filesystem::exists(backupAndRestoreCfgFilePath) &&
        !std::filesystem::is_empty(backupAndRestoreCfgFilePath))
    {
        return true;
    }

    return false;
}

/** @brief API to check if an action is required for given EEPROM path.
 *
 * System config JSON can contain pre-action, post-action etc. like actions
 * defined for an EEPROM path. The API will check if any such action is defined
 * for the EEPROM.
 *
 * @param[in] vpdFruPath - EEPROM path.
 * @param[in] action - Action to be checked.
 * @param[in] flowFlag - Denotes the flow w.r.t which the action should be
 * triggered.
 * @param[out] errCode - To set error code in case of error.
 * @return - True if action is defined for the flow, false otherwise.
 */
inline bool isActionRequired(const std::string& vpdFruPath,
                             const std::string& action,
                             const std::string& flowFlag, uint16_t& errCode)
{
    errCode = 0;
    if (vpdFruPath.empty() || action.empty() || flowFlag.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return false;
    }

    const auto configJsonResult = configManager->getJsonObj(vpdFruPath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return false;
    }

    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    if (!sysCfgJsonObj["frus"].contains(vpdFruPath))
    {
        errCode = error_code::FRU_PATH_NOT_FOUND;
        return false;
    }

    if ((sysCfgJsonObj["frus"][vpdFruPath].at(0)).contains(action))
    {
        if ((sysCfgJsonObj["frus"][vpdFruPath].at(0))[action].contains(
                flowFlag))
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief An API to return list of FRUs that needs GPIO polling.
 *
 * An API that checks for the FRUs that requires GPIO polling and returns
 * a list of FRUs that needs polling. Returns an empty list if there are
 * no FRUs that requires polling.
 *
 * @param[out] errCode - To set error codes in case of error.
 *
 * @return On success list of FRUs parameters that needs polling. On failure,
 * empty list.
 */
inline std::vector<std::string> getListOfGpioPollingFrus(uint16_t& errCode)
{
    std::vector<std::string> gpioPollingRequiredFrusList;
    errCode = 0;

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return gpioPollingRequiredFrusList;
    }

    const auto configJsonResult = configManager->getJsonObj();
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return gpioPollingRequiredFrusList;
    }

    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    for (const auto& fru : sysCfgJsonObj["frus"].items())
    {
        const auto fruPath = fru.key();

        bool isHotPluggableFru = isActionRequired(fruPath, "pollingRequired",
                                                  "hotPlugging", errCode);

        if (errCode)
        {
            Logger::getLoggerInstance()->logMessage(
                "Error while checking if action required for FRU [" +
                std::string(fruPath) +
                "], error : " + commonUtility::getErrCodeMsg(errCode));

            return gpioPollingRequiredFrusList;
        }

        if (isHotPluggableFru)
        {
            if (sysCfgJsonObj["frus"][fruPath]
                    .at(0)["pollingRequired"]["hotPlugging"]
                    .contains("gpioPresence"))
            {
                gpioPollingRequiredFrusList.push_back(fruPath);
            }
        }
    }

    return gpioPollingRequiredFrusList;
}

/**
 * @brief Get all related path(s) to update keyword value.
 *
 * Given FRU EEPROM path/Inventory path needs keyword's value update, this API
 * returns tuple of FRU EEPROM path, inventory path and redundant EEPROM path if
 * exists in the system config JSON.
 *
 * Note: If the inventory object path or redundant EEPROM path(s) are not found
 * in the system config JSON, corresponding fields will have empty value in the
 * returning tuple.
 *
 * @param[in,out] vpdPath - Inventory object path or FRU EEPROM path.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success returns tuple of EEPROM path, inventory path & redundant
 * path, on failure returns tuple with given input path alone.
 */
inline std::tuple<std::string, std::string, std::string>
    getAllPathsToUpdateKeyword(std::string vpdPath, uint16_t& errCode)
{
    types::Path inventoryObjPath;
    types::Path redundantFruPath;
    errCode = 0;

    if (vpdPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::make_tuple(vpdPath, inventoryObjPath, redundantFruPath);
    }

    // Get hardware path from system config JSON.
    const types::Path fruPath =
        jsonUtility::getFruPathFromJson(vpdPath, errCode);

    if (!fruPath.empty())
    {
        vpdPath = fruPath;

        // Get inventory object path from system config JSON
        inventoryObjPath =
            jsonUtility::getInventoryObjPathFromJson(fruPath, errCode);

        if (inventoryObjPath.empty())
        {
            if (errCode)
            {
                Logger::getLoggerInstance()->logMessage(
                    "Failed to get inventory path from JSON for [" + vpdPath +
                    "], error : " + commonUtility::getErrCodeMsg(errCode));
            }
            else
            {
                errCode = error_code::FRU_PATH_NOT_FOUND;
            }

            return std::make_tuple(vpdPath, inventoryObjPath, redundantFruPath);
        }

        // Get redundant hardware path if present in system config JSON
        redundantFruPath =
            jsonUtility::getRedundantEepromPathFromJson(fruPath, errCode);

        if (redundantFruPath.empty())
        {
            if (errCode)
            {
                Logger::getLoggerInstance()->logMessage(
                    "Failed to get redundant EEPROM path for FRU [" + fruPath +
                    "], error : " + commonUtility::getErrCodeMsg(errCode));

                errCode = error_code::ERROR_GETTING_REDUNDANT_PATH;
            }
            else
            {
                errCode = error_code::REDUNDANT_PATH_NOT_FOUND;
            }

            return std::make_tuple(vpdPath, inventoryObjPath, redundantFruPath);
        }
    }
    else if (errCode)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to get FRU path from JSON for [" + vpdPath +
            "], error : " + commonUtility::getErrCodeMsg(errCode));
    }
    else
    {
        errCode = error_code::NO_EEPROM_PATH;
    }

    return std::make_tuple(vpdPath, inventoryObjPath, redundantFruPath);
}

/**
 * @brief An API to get DBus service name.
 *
 * Given DBus inventory path, this API returns DBus service name if present in
 * the JSON.
 *
 * @param[in] inventoryPath - DBus inventory path.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success returns the service name present in the system config
 * JSON, otherwise empty string.
 *
 * Note: Caller has to handle in case of empty string received.
 */
inline std::string getServiceName(const std::string& inventoryPath,
                                  uint16_t& errCode)
{
    errCode = 0;
    if (inventoryPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return std::string{};
    }

    const auto configJsonResult = configManager->getJsonObj(inventoryPath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return std::string{};
    }

    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    const nlohmann::json& listOfFrus =
        sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& frus : listOfFrus.items())
    {
        for (const auto& inventoryItem : frus.value())
        {
            if (inventoryPath.compare(inventoryItem.value(
                    "inventoryPath", "")) == constants::STR_CMP_SUCCESS)
            {
                if (inventoryItem.contains("serviceName"))
                {
                    return inventoryItem.value("serviceName", "");
                }

                errCode = error_code::JSON_MISSING_SERVICE_NAME;
                return std::string{};
            }
        }
    }

    errCode = error_code::FRU_PATH_NOT_FOUND;
    return std::string{};
}

/**
 * @brief An API to check if a FRU is tagged as "powerOffOnly"
 *
 * Given the VPD FRU path, this API checks if the FRU VPD can be collected at
 * Chassis Power Off state only.
 *
 * @param[in] vpdFruPath - EEPROM path.
 * @param[out] errCode - To set error code for the error.
 * @return - True if FRU VPD can be collected at Chassis Power Off state only.
 *           False otherwise
 */
inline bool isFruPowerOffOnly(const std::string& vpdFruPath, uint16_t& errCode)
{
    errCode = 0;
    if (vpdFruPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return false;
    }

    const auto configJsonResult = configManager->getJsonObj(vpdFruPath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return false;
    }

    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    if (!sysCfgJsonObj["frus"].contains(vpdFruPath))
    {
        errCode = error_code::FRU_PATH_NOT_FOUND;
        return false;
    }

    return (
        (sysCfgJsonObj["frus"][vpdFruPath].at(0)).contains("powerOffOnly") &&
        (sysCfgJsonObj["frus"][vpdFruPath].at(0)["powerOffOnly"]));
}

/**
 * @brief API which tells if the FRU is replaceable at runtime
 *
 * @param[in] vpdFruPath - EEPROM path.
 * @param[out] errCode - to set error code in case of error.
 *
 * @return true if FRU is replaceable at runtime. false otherwise.
 */
inline bool isFruReplaceableAtRuntime(const std::string& vpdFruPath,
                                      uint16_t& errCode)
{
    errCode = 0;
    if (vpdFruPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return false;
    }

    const auto configJsonResult = configManager->getJsonObj(vpdFruPath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return false;
    }

    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    if (!sysCfgJsonObj["frus"].contains(vpdFruPath))
    {
        errCode = error_code::FRU_PATH_NOT_FOUND;
        return false;
    }

    return ((sysCfgJsonObj["frus"][vpdFruPath].at(0))
                .contains("replaceableAtRuntime") &&
            (sysCfgJsonObj["frus"][vpdFruPath].at(0)["replaceableAtRuntime"]));
}

/**
 * @brief API which tells if the FRU is replaceable at standby
 *
 * @param[in] vpdFruPath - EEPROM path.
 * @param[out] errCode - set error code in case of error.
 *
 * @return true if FRU is replaceable at standby. false otherwise.
 */
inline bool isFruReplaceableAtStandby(const std::string& vpdFruPath,
                                      uint16_t& errCode)
{
    errCode = 0;
    if (vpdFruPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return false;
    }

    const auto configJsonResult = configManager->getJsonObj(vpdFruPath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return false;
    }

    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    if (!sysCfgJsonObj["frus"].contains(vpdFruPath))
    {
        errCode = error_code::FRU_PATH_NOT_FOUND;
        return false;
    }

    return ((sysCfgJsonObj["frus"][vpdFruPath].at(0))
                .contains("replaceableAtStandby") &&
            (sysCfgJsonObj["frus"][vpdFruPath].at(0)["replaceableAtStandby"]));

    return false;
}

/**
 * @brief API to get list of FRUs replaceable at standby from JSON.
 *
 * The API will return a vector of FRUs inventory path which are replaceable at
 * standby.
 *
 * @param[out] errCode - To set error code in case of error.
 *
 * @return - On success, list of FRUs replaceable at standby. On failure, empty
 * vector.
 */
inline std::vector<std::string> getListOfFrusReplaceableAtStandby(
    uint16_t& errCode)
{
    std::vector<std::string> frusReplaceableAtStandby;
    errCode = 0;

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return frusReplaceableAtStandby;
    }

    const auto configJsonResult = configManager->getJsonObj();
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return frusReplaceableAtStandby;
    }

    // get the full system configuration JSON
    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    const nlohmann::json& fruList =
        sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& fru : fruList.items())
    {
        if (sysCfgJsonObj["frus"][fru.key()].at(0).value("replaceableAtStandby",
                                                         false))
        {
            const std::string& inventoryObjectPath =
                sysCfgJsonObj["frus"][fru.key()].at(0).value("inventoryPath",
                                                             "");

            if (!inventoryObjectPath.empty())
            {
                frusReplaceableAtStandby.emplace_back(inventoryObjectPath);
            }
        }
    }

    return frusReplaceableAtStandby;
}

/**
 * @brief API to get list of FRUs for which "monitorPresence" is true.
 *
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success, returns list of FRUs for which "monitorPresence" is true,
 * empty list on error.
 */
inline std::vector<types::Path> getFrusWithPresenceMonitoring(uint16_t& errCode)
{
    std::vector<types::Path> frusWithPresenceMonitoring;
    errCode = 0;

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return frusWithPresenceMonitoring;
    }

    const auto configJsonResult = configManager->getJsonObj();
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return frusWithPresenceMonitoring;
    }

    // get the full system configuration JSON
    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    const nlohmann::json& listOfFrus =
        sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& aFru : listOfFrus)
    {
        if (aFru.at(0).value("monitorPresence", false))
        {
            frusWithPresenceMonitoring.emplace_back(
                aFru.at(0).value("inventoryPath", ""));
        }
    }

    return frusWithPresenceMonitoring;
}

/**
 * @brief API which tells if the FRU's presence is handled
 *
 * For a given FRU, this API checks if it's presence is handled by vpd-manager
 * by checking the "handlePresence" tag.
 *
 * @param[in] vpdFruPath - EEPROM path.
 * @param[out] errCode - To set error code in case of failure.
 *
 * @return true if FRU presence is handled, false otherwise.
 */
inline bool isFruPresenceHandled(const std::string& vpdFruPath,
                                 uint16_t& errCode)
{
    errCode = 0;
    if (vpdFruPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    auto configManager = ConfigManager::getInstance();
    if (!configManager)
    {
        errCode = error_code::CONFIG_MANAGER_UNINITIALIZED;
        return false;
    }

    const auto configJsonResult = configManager->getJsonObj(vpdFruPath);
    if (!configJsonResult.has_value())
    {
        errCode = configJsonResult.error();
        return false;
    }

    // get the full system configuration JSON
    const nlohmann::json& sysCfgJsonObj = configJsonResult.value().get();

    if (!sysCfgJsonObj["frus"].contains(vpdFruPath))
    {
        errCode = error_code::FRU_PATH_NOT_FOUND;
        return false;
    }

    return sysCfgJsonObj["frus"][vpdFruPath].at(0).value("handlePresence",
                                                         true);
}
} // namespace jsonUtility
} // namespace vpd
