#pragma once

#include "error_codes.hpp"
#include "event_logger.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <gpiod.hpp>
#include <nlohmann/json.hpp>
#include <utility/common_utility.hpp>

#include <fstream>
#include <type_traits>
#include <unordered_map>

namespace vpd
{
namespace jsonUtility
{

// forward declaration of API for function map.
bool processSystemCmdTag(const nlohmann::json& i_parsedConfigJson,
                         const std::string& i_vpdFilePath,
                         const std::string& i_baseAction,
                         const std::string& i_flagToProcess);

// forward declaration of API for function map.
bool processGpioPresenceTag(
    const nlohmann::json& i_parsedConfigJson, const std::string& i_vpdFilePath,
    const std::string& i_baseAction, const std::string& i_flagToProcess);

// forward declaration of API for function map.
bool procesSetGpioTag(const nlohmann::json& i_parsedConfigJson,
                      const std::string& i_vpdFilePath,
                      const std::string& i_baseAction,
                      const std::string& i_flagToProcess);

// Function pointers to process tags from config JSON.
typedef bool (*functionPtr)(
    const nlohmann::json& i_parsedConfigJson, const std::string& i_vpdFilePath,
    const std::string& i_baseAction, const std::string& i_flagToProcess);

inline std::unordered_map<std::string, functionPtr> funcionMap{
    {"gpioPresence", processGpioPresenceTag},
    {"setGpio", procesSetGpioTag},
    {"systemCmd", processSystemCmdTag}};

/**
 * @brief API to read VPD offset from JSON file.
 *
 * @param[in] i_sysCfgJsonObj - Parsed system config JSON object.
 * @param[in] i_vpdFilePath - VPD file path.
 * @param[in] o_errCode - To set error code in case of error.
 * @return VPD offset if found in JSON, 0 otherwise.
 */
inline size_t getVPDOffset(const nlohmann::json& i_sysCfgJsonObj,
                           const std::string& i_vpdFilePath,
                           uint16_t& o_errCode)
{
    if (i_vpdFilePath.empty() || (i_sysCfgJsonObj.empty()) ||
        (!i_sysCfgJsonObj.contains("frus")))
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return 0;
    }

    if (i_sysCfgJsonObj["frus"].contains(i_vpdFilePath))
    {
        return i_sysCfgJsonObj["frus"][i_vpdFilePath].at(0).value("offset", 0);
    }

    const nlohmann::json& l_fruList =
        i_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& l_fru : l_fruList.items())
    {
        const auto l_fruPath = l_fru.key();

        // check if given path is redundant FRU path
        if (i_vpdFilePath == i_sysCfgJsonObj["frus"][l_fruPath].at(0).value(
                                 "redundantEeprom", ""))
        {
            // Return the offset of redundant EEPROM taken from JSON.
            return i_sysCfgJsonObj["frus"][l_fruPath].at(0).value("offset", 0);
        }
    }

    return 0;
}

/**
 * @brief API to parse respective JSON.
 *
 * @param[in] pathToJson - Path to JSON.
 * @return on success parsed JSON. On failure empty JSON object.
 *
 * Note: Caller has to handle it in case an empty JSON object is received.
 */
inline nlohmann::json getParsedJson(const std::string& pathToJson) noexcept
{
    try
    {
        if (pathToJson.empty())
        {
            throw std::runtime_error("Path to JSON is missing");
        }

        if (!std::filesystem::exists(pathToJson) ||
            std::filesystem::is_empty(pathToJson))
        {
            throw std::runtime_error(
                "File does not exist or empty file: [" + pathToJson + "]");
        }

        std::ifstream l_jsonFile(pathToJson);
        if (!l_jsonFile)
        {
            throw std::runtime_error(
                "Failed to access Json path = " + pathToJson);
        }

        return nlohmann::json::parse(l_jsonFile);
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to parse JSON file, error: " + std::string(l_ex.what()));
    }

    return nlohmann::json{};
}

/**
 * @brief Get inventory object path from system config JSON.
 *
 * Given either D-bus inventory path/FRU EEPROM path/redundant EEPROM path,
 * this API returns D-bus inventory path if present in JSON.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object
 * @param[in] i_vpdPath - Path to where VPD is stored.
 * @param[in] o_errCode - To set error code in case of error.
 *
 * @return On success a valid path is returned, on failure an empty string is
 * returned.
 *
 * Note: Caller has to handle it in case an empty string is received.
 */
inline std::string getInventoryObjPathFromJson(
    const nlohmann::json& i_sysCfgJsonObj, const std::string& i_vpdPath,
    uint16_t& o_errCode) noexcept
{
    if (i_vpdPath.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }

    if (!i_sysCfgJsonObj.contains("frus"))
    {
        o_errCode = error_code::INVALID_JSON;
        return std::string{};
    }

    // check if given path is FRU path
    if (i_sysCfgJsonObj["frus"].contains(i_vpdPath))
    {
        return i_sysCfgJsonObj["frus"][i_vpdPath].at(0).value(
            "inventoryPath", "");
    }

    const nlohmann::json& l_fruList =
        i_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& l_fru : l_fruList.items())
    {
        const auto l_fruPath = l_fru.key();
        const auto l_invObjPath =
            i_sysCfgJsonObj["frus"][l_fruPath].at(0).value("inventoryPath", "");

        // check if given path is redundant FRU path or inventory path
        if (i_vpdPath == i_sysCfgJsonObj["frus"][l_fruPath].at(0).value(
                             "redundantEeprom", "") ||
            (i_vpdPath == l_invObjPath))
        {
            return l_invObjPath;
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
 * @param[in] i_parsedConfigJson - config JSON
 * @param[in] i_vpdFilePath - EEPROM file path
 * @param[in] i_flagToProcess - To identify which flag(s) needs to be processed
 * under PostFailAction tag of config JSON.
 * @return - success or failure
 */
inline bool executePostFailAction(const nlohmann::json& i_parsedConfigJson,
                                  const std::string& i_vpdFilePath,
                                  const std::string& i_flagToProcess)
{
    try
    {
        if (i_parsedConfigJson.empty() || i_vpdFilePath.empty() ||
            i_flagToProcess.empty())
        {
            throw std::runtime_error(
                "Invalid parameters. Abort processing for post fail action");
        }

        if (!(i_parsedConfigJson["frus"][i_vpdFilePath].at(0))["postFailAction"]
                 .contains(i_flagToProcess))
        {
            throw std::runtime_error(
                "Config JSON missing flag " + i_flagToProcess +
                " to execute post fail action for path = " + i_vpdFilePath);
        }

        for (const auto& l_tags : (i_parsedConfigJson["frus"][i_vpdFilePath].at(
                 0))["postFailAction"][i_flagToProcess]
                                      .items())
        {
            auto itrToFunction = funcionMap.find(l_tags.key());
            if (itrToFunction != funcionMap.end())
            {
                if (!itrToFunction->second(i_parsedConfigJson, i_vpdFilePath,
                                           "postFailAction", i_flagToProcess))
                {
                    return false;
                }
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Execute post fail action failed. Error : " +
                            std::string(l_ex.what()));
        return false;
    }

    return true;
}

/**
 * @brief Process "systemCmd" tag for a given FRU.
 *
 * The API will process "systemCmd" tag if it is defined in the config
 * JSON for the given FRU.
 *
 * @param[in] i_parsedConfigJson - config JSON
 * @param[in] i_vpdFilePath - EEPROM file path
 * @param[in] i_baseAction - Base action for which this tag has been called.
 * @param[in] i_flagToProcess - Flag nested under the base action for which this
 * tag has been called.
 * @return Execution status.
 */
inline bool processSystemCmdTag(
    const nlohmann::json& i_parsedConfigJson, const std::string& i_vpdFilePath,
    const std::string& i_baseAction, const std::string& i_flagToProcess)
{
    try
    {
        if (i_vpdFilePath.empty() || i_parsedConfigJson.empty() ||
            i_baseAction.empty() || i_flagToProcess.empty())
        {
            throw std::runtime_error(
                std::string(__FUNCTION__) +
                " Invalid parameter. Abort processing of processSystemCmd.");
        }

        if (!((i_parsedConfigJson["frus"][i_vpdFilePath].at(
                   0)[i_baseAction][i_flagToProcess]["systemCmd"])
                  .contains("cmd")))
        {
            throw JsonException(
                std::string(__FUNCTION__) +
                " Config JSON missing required information to execute system command for EEPROM " +
                i_vpdFilePath);
        }

        const std::string& l_systemCommand =
            i_parsedConfigJson["frus"][i_vpdFilePath].at(
                0)[i_baseAction][i_flagToProcess]["systemCmd"]["cmd"];

        commonUtility::executeCmd(l_systemCommand);
        return true;
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        return false;
    }
}

/**
 * @brief Checks for presence of a given FRU using GPIO line.
 *
 * This API returns the presence information of the FRU corresponding to the
 * given VPD file path by setting the presence pin.
 *
 * @param[in] i_parsedConfigJson - config JSON
 * @param[in] i_vpdFilePath - EEPROM file path
 * @param[in] i_baseAction - Base action for which this tag has been called.
 * @param[in] i_flagToProcess - Flag nested under the base action for which this
 * tag has been called.
 * @return Execution status.
 */
inline bool processGpioPresenceTag(
    const nlohmann::json& i_parsedConfigJson, const std::string& i_vpdFilePath,
    const std::string& i_baseAction, const std::string& i_flagToProcess)
{
    std::string l_presencePinName;
    try
    {
        if (i_vpdFilePath.empty() || i_parsedConfigJson.empty() ||
            i_baseAction.empty() || i_flagToProcess.empty())
        {
            throw std::runtime_error(
                std::string(__FUNCTION__) +
                "Invalid parameter. Abort processing of processGpioPresence tag");
        }

        if (!(((i_parsedConfigJson["frus"][i_vpdFilePath].at(
                    0)[i_baseAction][i_flagToProcess]["gpioPresence"])
                   .contains("pin")) &&
              ((i_parsedConfigJson["frus"][i_vpdFilePath].at(
                    0)[i_baseAction][i_flagToProcess]["gpioPresence"])
                   .contains("value"))))
        {
            throw JsonException(
                std::string(__FUNCTION__) +
                "Config JSON missing required information to detect presence for EEPROM " +
                i_vpdFilePath);
        }

        // get the pin name
        l_presencePinName = i_parsedConfigJson["frus"][i_vpdFilePath].at(
            0)[i_baseAction][i_flagToProcess]["gpioPresence"]["pin"];

        // get the pin value
        uint8_t l_presencePinValue =
            i_parsedConfigJson["frus"][i_vpdFilePath].at(
                0)[i_baseAction][i_flagToProcess]["gpioPresence"]["value"];

        gpiod::line l_presenceLine = gpiod::find_line(l_presencePinName);

        if (!l_presenceLine)
        {
            throw GpioException("Couldn't find the GPIO line.");
        }

        l_presenceLine.request({"Read the presence line",
                                gpiod::line_request::DIRECTION_INPUT, 0});

        return (l_presencePinValue == l_presenceLine.get_value());
    }
    catch (const std::exception& l_ex)
    {
        // No need to continue in case of JSON failure or Firmware error
        // as these are errors internal to the code and in that case the FRU
        // should not be processed. Any other error is considered as external
        // error in this case and a try to read the EEPROM should be done.
        if (EventLogger::getErrorType(l_ex) == types::ErrorType::JsonFailure ||
            EventLogger::getErrorType(l_ex) == types::ErrorType::FirmwareError)
        {
            EventLogger::createSyncPel(
                EventLogger::getErrorType(l_ex),
                types::SeverityType::Informational, __FILE__, __FUNCTION__, 0,
                EventLogger::getErrorMsg(l_ex), std::nullopt, std::nullopt,
                std::nullopt, std::nullopt);
            return false;
        }

        std::string l_errMsg = "Exception on GPIO line: ";
        l_errMsg += l_presencePinName;
        l_errMsg += " Reason: ";
        l_errMsg += l_ex.what();
        l_errMsg += " File: " + i_vpdFilePath + " Pel Logged";

        uint16_t l_errCode = 0;

        // ToDo -- Update Internal Rc code.
        EventLogger::createAsyncPelWithInventoryCallout(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            {{getInventoryObjPathFromJson(i_parsedConfigJson, i_vpdFilePath,
                                          l_errCode),
              types::CalloutPriority::High}},
            std::source_location::current().file_name(),
            std::source_location::current().function_name(), 0, l_errMsg,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);

        logging::logMessage(l_errMsg);

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
 * @param[in] i_parsedConfigJson - config JSON
 * @param[in] i_vpdFilePath - EEPROM file path
 * @param[in] i_baseAction - Base action for which this tag has been called.
 * @param[in] i_flagToProcess - Flag nested under the base action for which this
 * tag has been called.
 * @return Execution status.
 */
inline bool procesSetGpioTag(
    const nlohmann::json& i_parsedConfigJson, const std::string& i_vpdFilePath,
    const std::string& i_baseAction, const std::string& i_flagToProcess)
{
    std::string l_pinName;
    try
    {
        if (i_vpdFilePath.empty() || i_parsedConfigJson.empty() ||
            i_baseAction.empty() || i_flagToProcess.empty())
        {
            throw std::runtime_error(
                std::string(__FUNCTION__) +
                " Invalid parameter. Abort processing of procesSetGpio.");
        }

        if (!(((i_parsedConfigJson["frus"][i_vpdFilePath].at(
                    0)[i_baseAction][i_flagToProcess]["setGpio"])
                   .contains("pin")) &&
              ((i_parsedConfigJson["frus"][i_vpdFilePath].at(
                    0)[i_baseAction][i_flagToProcess]["setGpio"])
                   .contains("value"))))
        {
            throw JsonException(
                std::string(__FUNCTION__) +
                " Config JSON missing required information to set gpio line for EEPROM " +
                i_vpdFilePath);
        }

        l_pinName = i_parsedConfigJson["frus"][i_vpdFilePath].at(
            0)[i_baseAction][i_flagToProcess]["setGpio"]["pin"];

        // Get the value to set
        uint8_t l_pinValue = i_parsedConfigJson["frus"][i_vpdFilePath].at(
            0)[i_baseAction][i_flagToProcess]["setGpio"]["value"];

        logging::logMessage(
            "Setting GPIO: " + l_pinName + " to " + std::to_string(l_pinValue));

        gpiod::line l_outputLine = gpiod::find_line(l_pinName);

        if (!l_outputLine)
        {
            throw GpioException("Couldn't find GPIO line.");
        }

        l_outputLine.request(
            {"FRU Action", ::gpiod::line_request::DIRECTION_OUTPUT, 0},
            l_pinValue);
        return true;
    }
    catch (const std::exception& l_ex)
    {
        if (EventLogger::getErrorType(l_ex) != types::ErrorType::GpioError)
        {
            EventLogger::createSyncPel(
                EventLogger::getErrorType(l_ex),
                types::SeverityType::Informational, __FILE__, __FUNCTION__, 0,
                EventLogger::getErrorMsg(l_ex), std::nullopt, std::nullopt,
                std::nullopt, std::nullopt);
        }
        else
        {
            std::string l_errMsg = "Exception on GPIO line: ";
            l_errMsg += l_pinName;
            l_errMsg += " Reason: ";
            l_errMsg += l_ex.what();
            l_errMsg += " File: " + i_vpdFilePath + " Pel Logged";

            uint16_t l_errCode = 0;

            // ToDo -- Update Internal RC code
            EventLogger::createAsyncPelWithInventoryCallout(
                EventLogger::getErrorType(l_ex),
                types::SeverityType::Informational,
                {{getInventoryObjPathFromJson(i_parsedConfigJson, i_vpdFilePath,
                                              l_errCode),
                  types::CalloutPriority::High}},
                std::source_location::current().file_name(),
                std::source_location::current().function_name(), 0, l_errMsg,
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);

            logging::logMessage(l_errMsg);
        }

        return false;
    }
}

/**
 * @brief Process any action, if defined in config JSON.
 *
 * If any FRU(s) requires any special handling, then this base action can be
 * defined for that FRU in the config JSON, processing of which will be handled
 * in this API.
 * Examples of action - preAction, PostAction etc.
 *
 * @param[in] i_parsedConfigJson - config JSON
 * @param[in] i_action - Base action to be performed.
 * @param[in] i_vpdFilePath - EEPROM file path
 * @param[in] i_flagToProcess - To identify which flag(s) needs to be processed
 * under PreAction tag of config JSON.
 * @return - success or failure
 */
inline bool executeBaseAction(
    const nlohmann::json& i_parsedConfigJson, const std::string& i_action,
    const std::string& i_vpdFilePath, const std::string& i_flagToProcess)
{
    try
    {
        if (i_flagToProcess.empty() || i_action.empty() ||
            i_vpdFilePath.empty() || !i_parsedConfigJson.contains("frus"))
        {
            throw std::runtime_error(
                std::string(__FUNCTION__) + " Invalid parameter");
        }

        if (!i_parsedConfigJson["frus"].contains(i_vpdFilePath))
        {
            throw JsonException(std::string(__FUNCTION__) + " File path: " +
                                i_vpdFilePath + " not found in JSON");
        }

        if (!i_parsedConfigJson["frus"][i_vpdFilePath].at(0).contains(i_action))
        {
            throw JsonException(
                std::string(__FUNCTION__) + " Action [" + i_action +
                "] not defined for file path:" + i_vpdFilePath);
        }

        if (!(i_parsedConfigJson["frus"][i_vpdFilePath].at(0))[i_action]
                 .contains(i_flagToProcess))
        {
            throw JsonException(
                std::string(__FUNCTION__) + "Config JSON missing flag [" +
                i_flagToProcess +
                "] to execute action for path = " + i_vpdFilePath);
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        return false;
    }

    const nlohmann::json& l_tagsJson =
        (i_parsedConfigJson["frus"][i_vpdFilePath].at(
            0))[i_action][i_flagToProcess];

    for (const auto& l_tag : l_tagsJson.items())
    {
        auto itrToFunction = funcionMap.find(l_tag.key());
        if (itrToFunction != funcionMap.end())
        {
            if (!itrToFunction->second(i_parsedConfigJson, i_vpdFilePath,
                                       i_action, i_flagToProcess))
            {
                // In case any of the tag fails to execute. Mark action
                // as failed for that flag.
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Get redundant FRU path from system config JSON
 *
 * Given either D-bus inventory path/FRU path/redundant FRU path, this
 * API returns the redundant FRU path taken from "redundantEeprom" tag from
 * system config JSON.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 * @param[in] i_vpdPath - Path to where VPD is stored.
 *
 * @return On success return valid path, on failure return empty string.
 */
inline std::string getRedundantEepromPathFromJson(
    const nlohmann::json& i_sysCfgJsonObj,
    const std::string& i_vpdPath) noexcept
{
    try
    {
        if (i_vpdPath.empty())
        {
            throw std::runtime_error("Path parameter is empty.");
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error("Missing frus tag in system config JSON.");
        }

        // check if given path is FRU path
        if (i_sysCfgJsonObj["frus"].contains(i_vpdPath))
        {
            return i_sysCfgJsonObj["frus"][i_vpdPath].at(0).value(
                "redundantEeprom", "");
        }

        const nlohmann::json& l_fruList =
            i_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

        for (const auto& l_fru : l_fruList.items())
        {
            const std::string& l_fruPath = l_fru.key();
            const std::string& l_redundantFruPath =
                i_sysCfgJsonObj["frus"][l_fruPath].at(0).value(
                    "redundantEeprom", "");

            // check if given path is inventory path or redundant FRU path
            if ((i_sysCfgJsonObj["frus"][l_fruPath].at(0).value(
                     "inventoryPath", "") == i_vpdPath) ||
                (l_redundantFruPath == i_vpdPath))
            {
                return l_redundantFruPath;
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to get redundant EEPROM path, error: " +
                            std::string(l_ex.what()));
    }

    return std::string();
}

/**
 * @brief Get FRU EEPROM path from system config JSON
 *
 * Given either D-bus inventory path/FRU EEPROM path/redundant EEPROM path,
 * this API returns FRU EEPROM path if present in JSON.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object
 * @param[in] i_vpdPath - Path to where VPD is stored.
 *
 * @return On success return valid path, on failure return empty string.
 */
inline std::string getFruPathFromJson(const nlohmann::json& i_sysCfgJsonObj,
                                      const std::string& i_vpdPath) noexcept
{
    try
    {
        if (i_vpdPath.empty())
        {
            throw std::runtime_error("Path parameter is empty.");
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error("Missing frus tag in system config JSON.");
        }

        // check if given path is FRU path
        if (i_sysCfgJsonObj["frus"].contains(i_vpdPath))
        {
            return i_vpdPath;
        }

        const nlohmann::json& l_fruList =
            i_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

        for (const auto& l_fru : l_fruList.items())
        {
            const auto l_fruPath = l_fru.key();

            // check if given path is redundant FRU path or inventory path
            if (i_vpdPath == i_sysCfgJsonObj["frus"][l_fruPath].at(0).value(
                                 "redundantEeprom", "") ||
                (i_vpdPath == i_sysCfgJsonObj["frus"][l_fruPath].at(0).value(
                                  "inventoryPath", "")))
            {
                return l_fruPath;
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to get FRU path from JSON, error: " +
                            std::string(l_ex.what()));
    }

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
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 *
 * @return true if backup and restore is required, false otherwise.
 */
inline bool isBackupAndRestoreRequired(const nlohmann::json& i_sysCfgJsonObj)
{
    try
    {
        const std::string& l_backupAndRestoreCfgFilePath =
            i_sysCfgJsonObj.value("backupRestoreConfigPath", "");
        if (!l_backupAndRestoreCfgFilePath.empty() &&
            std::filesystem::exists(l_backupAndRestoreCfgFilePath) &&
            !std::filesystem::is_empty(l_backupAndRestoreCfgFilePath))
        {
            return true;
        }
    }
    catch (std::exception& ex)
    {
        logging::logMessage(ex.what());
    }
    return false;
}

/** @brief API to check if an action is required for given EEPROM path.
 *
 * System config JSON can contain pre-action, post-action etc. like actions
 * defined for an EEPROM path. The API will check if any such action is defined
 * for the EEPROM.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 * @param[in] i_vpdFruPath - EEPROM path.
 * @param[in] i_action - Action to be checked.
 * @param[in] i_flowFlag - Denotes the flow w.r.t which the action should be
 * triggered.
 * @return - True if action is defined for the flow, false otherwise.
 */
inline bool isActionRequired(
    const nlohmann::json& i_sysCfgJsonObj, const std::string& i_vpdFruPath,
    const std::string& i_action, const std::string& i_flowFlag)
{
    if (i_vpdFruPath.empty() || i_action.empty() || i_flowFlag.empty())
    {
        logging::logMessage("Invalid parameters recieved.");
        return false;
    }

    if (!i_sysCfgJsonObj.contains("frus"))
    {
        logging::logMessage("Invalid JSON object recieved.");
        return false;
    }

    if (!i_sysCfgJsonObj["frus"].contains(i_vpdFruPath))
    {
        logging::logMessage(
            "JSON object does not contain EEPROM path " + i_vpdFruPath);
        return false;
    }

    if ((i_sysCfgJsonObj["frus"][i_vpdFruPath].at(0)).contains(i_action))
    {
        if ((i_sysCfgJsonObj["frus"][i_vpdFruPath].at(0))[i_action].contains(
                i_flowFlag))
        {
            return true;
        }

        logging::logMessage("Flow flag: [" + i_flowFlag +
                            "], not found in JSON for path: " + i_vpdFruPath +
                            " while checking for action: " + i_action);
        return false;
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
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 *
 * @return On success list of FRUs parameters that needs polling. On failure,
 * empty list.
 */
inline std::vector<std::string> getListOfGpioPollingFrus(
    const nlohmann::json& i_sysCfgJsonObj) noexcept
{
    std::vector<std::string> l_gpioPollingRequiredFrusList;

    try
    {
        if (i_sysCfgJsonObj.empty())
        {
            throw std::runtime_error("Invalid Parameters");
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error(
                "Missing frus section in system config JSON");
        }

        for (const auto& l_fru : i_sysCfgJsonObj["frus"].items())
        {
            const auto l_fruPath = l_fru.key();

            if (isActionRequired(i_sysCfgJsonObj, l_fruPath, "pollingRequired",
                                 "hotPlugging"))
            {
                if (i_sysCfgJsonObj["frus"][l_fruPath]
                        .at(0)["pollingRequired"]["hotPlugging"]
                        .contains("gpioPresence"))
                {
                    l_gpioPollingRequiredFrusList.push_back(l_fruPath);
                }
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to get list of GPIO polling FRUs, error: " +
                            std::string(l_ex.what()));
    }

    return l_gpioPollingRequiredFrusList;
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
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 * @param[in,out] io_vpdPath - Inventory object path or FRU EEPROM path.
 *
 * @return On success returns tuple of EEPROM path, inventory path & redundant
 * path, on failure returns tuple with given input path alone.
 */
inline std::tuple<std::string, std::string, std::string>
    getAllPathsToUpdateKeyword(const nlohmann::json& i_sysCfgJsonObj,
                               std::string io_vpdPath)
{
    types::Path l_inventoryObjPath;
    types::Path l_redundantFruPath;
    try
    {
        if (!i_sysCfgJsonObj.empty())
        {
            // Get hardware path from system config JSON.
            const types::Path l_fruPath =
                jsonUtility::getFruPathFromJson(i_sysCfgJsonObj, io_vpdPath);

            if (!l_fruPath.empty())
            {
                io_vpdPath = l_fruPath;

                uint16_t l_errCode = 0;

                // Get inventory object path from system config JSON
                l_inventoryObjPath = jsonUtility::getInventoryObjPathFromJson(
                    i_sysCfgJsonObj, l_fruPath, l_errCode);

                // Get redundant hardware path if present in system config JSON
                l_redundantFruPath =
                    jsonUtility::getRedundantEepromPathFromJson(i_sysCfgJsonObj,
                                                                l_fruPath);
            }
        }
    }
    catch (const std::exception& l_exception)
    {
        logging::logMessage(
            "Failed to get all paths to update keyword value, error " +
            std::string(l_exception.what()));
    }
    return std::make_tuple(io_vpdPath, l_inventoryObjPath, l_redundantFruPath);
}

/**
 * @brief An API to get DBus service name.
 *
 * Given DBus inventory path, this API returns DBus service name if present in
 * the JSON.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 * @param[in] l_inventoryPath - DBus inventory path.
 *
 * @return On success returns the service name present in the system config
 * JSON, otherwise empty string.
 *
 * Note: Caller has to handle in case of empty string received.
 */
inline std::string getServiceName(const nlohmann::json& i_sysCfgJsonObj,
                                  const std::string& l_inventoryPath)
{
    try
    {
        if (l_inventoryPath.empty())
        {
            throw std::runtime_error("Path parameter is empty.");
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error("Missing frus tag in system config JSON.");
        }

        const nlohmann::json& l_listOfFrus =
            i_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

        for (const auto& l_frus : l_listOfFrus.items())
        {
            for (const auto& l_inventoryItem : l_frus.value())
            {
                if (l_inventoryPath.compare(l_inventoryItem["inventoryPath"]) ==
                    constants::STR_CMP_SUCCESS)
                {
                    return l_inventoryItem["serviceName"];
                }
            }
        }
        throw std::runtime_error(
            "Inventory path not found in the system config JSON");
    }
    catch (const std::exception& l_exception)
    {
        logging::logMessage(
            "Error while getting DBus service name for given path " +
            l_inventoryPath + ", error: " + std::string(l_exception.what()));
        // TODO:log PEL
    }
    return std::string{};
}

/**
 * @brief An API to check if a FRU is tagged as "powerOffOnly"
 *
 * Given the system config JSON and VPD FRU path, this API checks if the FRU
 * VPD can be collected at Chassis Power Off state only.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 * @param[in] i_vpdFruPath - EEPROM path.
 * @return - True if FRU VPD can be collected at Chassis Power Off state only.
 *           False otherwise
 */
inline bool isFruPowerOffOnly(const nlohmann::json& i_sysCfgJsonObj,
                              const std::string& i_vpdFruPath)
{
    if (i_vpdFruPath.empty())
    {
        logging::logMessage("FRU path is empty.");
        return false;
    }

    if (!i_sysCfgJsonObj.contains("frus"))
    {
        logging::logMessage("Missing frus tag in system config JSON.");
        return false;
    }

    if (!i_sysCfgJsonObj["frus"].contains(i_vpdFruPath))
    {
        logging::logMessage("JSON object does not contain EEPROM path \'" +
                            i_vpdFruPath + "\'");
        return false;
    }

    return ((i_sysCfgJsonObj["frus"][i_vpdFruPath].at(0))
                .contains("powerOffOnly") &&
            (i_sysCfgJsonObj["frus"][i_vpdFruPath].at(0)["powerOffOnly"]));
}

/**
 * @brief API which tells if the FRU is replaceable at runtime
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 * @param[in] i_vpdFruPath - EEPROM path.
 *
 * @return true if FRU is replaceable at runtime. false otherwise.
 */
inline bool isFruReplaceableAtRuntime(const nlohmann::json& i_sysCfgJsonObj,
                                      const std::string& i_vpdFruPath)
{
    try
    {
        if (i_vpdFruPath.empty())
        {
            throw std::runtime_error("Given FRU path is empty.");
        }

        if (i_sysCfgJsonObj.empty() || (!i_sysCfgJsonObj.contains("frus")))
        {
            throw std::runtime_error("Invalid system config JSON object.");
        }

        return ((i_sysCfgJsonObj["frus"][i_vpdFruPath].at(0))
                    .contains("replaceableAtRuntime") &&
                (i_sysCfgJsonObj["frus"][i_vpdFruPath].at(
                    0)["replaceableAtRuntime"]));
    }
    catch (const std::exception& l_error)
    {
        // TODO: Log PEL
        logging::logMessage(l_error.what());
    }

    return false;
}

/**
 * @brief API which tells if the FRU is replaceable at standby
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 * @param[in] i_vpdFruPath - EEPROM path.
 *
 * @return true if FRU is replaceable at standby. false otherwise.
 */
inline bool isFruReplaceableAtStandby(const nlohmann::json& i_sysCfgJsonObj,
                                      const std::string& i_vpdFruPath)
{
    try
    {
        if (i_vpdFruPath.empty())
        {
            throw std::runtime_error("Given FRU path is empty.");
        }

        if (i_sysCfgJsonObj.empty() || (!i_sysCfgJsonObj.contains("frus")))
        {
            throw std::runtime_error("Invalid system config JSON object.");
        }

        return ((i_sysCfgJsonObj["frus"][i_vpdFruPath].at(0))
                    .contains("replaceableAtStandby") &&
                (i_sysCfgJsonObj["frus"][i_vpdFruPath].at(
                    0)["replaceableAtStandby"]));
    }
    catch (const std::exception& l_error)
    {
        // TODO: Log PEL
        logging::logMessage(l_error.what());
    }

    return false;
}

/**
 * @brief API to get list of FRUs replaceable at standby from JSON.
 *
 * The API will return a vector of FRUs inventory path which are replaceable at
 * standby.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 *
 * @return - On success, list of FRUs replaceable at standby. On failure, empty
 * vector.
 */
inline std::vector<std::string> getListOfFrusReplaceableAtStandby(
    const nlohmann::json& i_sysCfgJsonObj) noexcept
{
    std::vector<std::string> l_frusReplaceableAtStandby;

    try
    {
        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error("Missing frus tag in system config JSON.");
        }

        const nlohmann::json& l_fruList =
            i_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

        for (const auto& l_fru : l_fruList.items())
        {
            if (i_sysCfgJsonObj["frus"][l_fru.key()].at(0).value(
                    "replaceableAtStandby", false))
            {
                const std::string& l_inventoryObjectPath =
                    i_sysCfgJsonObj["frus"][l_fru.key()].at(0).value(
                        "inventoryPath", "");

                if (!l_inventoryObjectPath.empty())
                {
                    l_frusReplaceableAtStandby.emplace_back(
                        l_inventoryObjectPath);
                }
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to get list of FRUs replaceable at standby, error: " +
            std::string(l_ex.what()));
    }

    return l_frusReplaceableAtStandby;
}

/**
 * @brief API to select powerVS JSON based on system IM.
 *
 * The API selects respective JSON based on system IM, parse it and return the
 * JSON object. Empty JSON will be returned in case of any error. Caller needs
 * to handle empty value.
 *
 * @param[in] i_imValue - IM value of the system.
 * @return Parsed JSON object, empty JSON otherwise.
 */
inline nlohmann::json getPowerVsJson(const types::BinaryVector& i_imValue)
{
    try
    {
        if ((i_imValue.at(0) == constants::HEX_VALUE_50) &&
            (i_imValue.at(1) == constants::HEX_VALUE_00) &&
            (i_imValue.at(2) == constants::HEX_VALUE_30))
        {
            return jsonUtility::getParsedJson(constants::power_vs_50003_json);
        }
        else if (i_imValue.at(0) == constants::HEX_VALUE_50 &&
                 (i_imValue.at(1) == constants::HEX_VALUE_00) &&
                 (i_imValue.at(2) == constants::HEX_VALUE_10))
        {
            return jsonUtility::getParsedJson(constants::power_vs_50001_json);
        }
        return nlohmann::json{};
    }
    catch (const std::exception& l_ex)
    {
        return nlohmann::json{};
    }
}

/**
 * @brief API to get list of FRUs for which "monitorPresence" is true.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 *
 * @return On success, returns list of FRUs for which "monitorPresence" is true,
 * empty list on error.
 */
inline std::vector<types::Path> getFrusWithPresenceMonitoring(
    const nlohmann::json& i_sysCfgJsonObj) noexcept
{
    std::vector<types::Path> l_frusWithPresenceMonitoring;
    try
    {
        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw JsonException("Missing frus tag in system config JSON.");
        }

        const nlohmann::json& l_listOfFrus =
            i_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

        for (const auto& l_aFru : l_listOfFrus)
        {
            if (l_aFru.at(0).value("monitorPresence", false))
            {
                l_frusWithPresenceMonitoring.emplace_back(
                    l_aFru.at(0).value("inventoryPath", ""));
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to get list of FRUs with presence monitoring, error: " +
            std::string(l_ex.what()));
    }
    return l_frusWithPresenceMonitoring;
}

/**
 * @brief API which tells if the FRU's presence is handled
 *
 * For a given FRU, this API checks if it's presence is handled by vpd-manager
 * by checking the "handlePresence" tag.
 *
 * @param[in] i_sysCfgJsonObj - System config JSON object.
 * @param[in] i_vpdFruPath - EEPROM path.
 *
 * @return true if FRU presence is handled, false otherwise.
 */
inline bool isFruPresenceHandled(const nlohmann::json& i_sysCfgJsonObj,
                                 const std::string& i_vpdFruPath)
{
    try
    {
        if (i_vpdFruPath.empty())
        {
            throw std::runtime_error("Given FRU path is empty.");
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw JsonException("Invalid system config JSON object.");
        }

        if (!i_sysCfgJsonObj["frus"].contains(i_vpdFruPath))
        {
            logging::logMessage(
                "JSON object does not contain EEPROM path " + i_vpdFruPath);
            return false;
        }

        return i_sysCfgJsonObj["frus"][i_vpdFruPath].at(0).value(
            "handlePresence", true);
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to check if FRU's presence is handled, reason:" +
            std::string(l_ex.what()));
    }

    return false;
}
} // namespace jsonUtility
} // namespace vpd
