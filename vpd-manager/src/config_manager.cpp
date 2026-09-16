#include "config_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "utility/common_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>

namespace vpd
{

// Static singleton instance — null until initialize() is called
std::atomic<std::shared_ptr<ConfigManager>> ConfigManager::instance{nullptr};

std::shared_ptr<ConfigManager> ConfigManager::getInstance() noexcept
{
    return instance.load();
}

std::shared_ptr<ConfigManager> ConfigManager::initialize(
    [[maybe_unused]] const ManagerPassKey& key,
    const std::string& sysConfigJsonPath)
{
    std::shared_ptr<ConfigManager> newInstance{nullptr};
    try
    {
        // Build the new instance entirely on the side so that any concurrent
        // reader holding a snapshot of instance continues to see the old,
        // fully-populated data right up until the atomic pointer swap below.
        newInstance = std::shared_ptr<ConfigManager>(new ConfigManager());
        newInstance->loadJson(sysConfigJsonPath);
    }
    catch (const std::exception& ex)
    {
        // if ConfigManager is not yet initialized, re-throw the exception so
        // that Manager initialization also fails
        if (instance.load() == nullptr)
        {
            throw;
        }
        else
        {
            // ConfigManager initialization failed with current JSON.
            // Log a PEL and continue so that vpd-manager can proceed
            // with previously loaded JSON
            Logger::getLoggerInstance()->logMessage(
                std::format(
                    "Failed to load JSON from path {}. Error: {}. Continuing with previously loaded JSON",
                    sysConfigJsonPath, ex.what()),
                PlaceHolder::ASYNC_PEL,
                types::PelInfoTuple{types::ErrorType::FirmwareError,
                                    types::SeverityType::Warning, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt, std::nullopt});

            // return the existing ConfigManager instance
            return instance.load();
        }
    }

    // Single atomic pointer swap — readers see either the old complete
    // state or the new complete state, never an intermediate state.
    instance.store(newInstance);
    return instance.load();
}

void ConfigManager::loadJson(const std::string& sysConfigJsonPath)
{
    uint16_t errCode{constants::VALUE_0};

    systemConfigJson = getParsedJson(sysConfigJsonPath, errCode);

    if (errCode != constants::VALUE_0)
    {
        throw JsonException{std::format(
            "ConfigManager failed to load JSON from path {}. Error: {}",
            sysConfigJsonPath, commonUtility::getErrCodeMsg(errCode))};
    }

    // Validate the system configuration JSON
    JsonValidator::validateConfigJson(systemConfigJson);

    buildConfigMaps();

    // Validate the chassis-specific JSONs
    validateChassisSpecificJsons();
}

std::expected<std::reference_wrapper<const nlohmann::json>, error_code>
    ConfigManager::getJsonObj(
        const std::optional<std::string>& vpdPath) const noexcept
{
    if (!vpdPath)
    {
        return std::cref(systemConfigJson);
    }

    std::string chassisId{};

    if ((*vpdPath).starts_with(std::string_view(constants::pimPath)))
    {
        chassisId = getChassisId(*vpdPath);

        if (chassisId.empty())
        {
            // see if the given object path is in the system configuration JSON
            // if yes, return the system configuration JSON
            if (isInventoryPathInJson(*vpdPath))
            {
                return std::cref(systemConfigJson);
            }
        }
    }
    else if (const auto itr = eepromToChassisIdMap.find(*vpdPath);
             itr != eepromToChassisIdMap.end())
    {
        chassisId = itr->second;
    }
    else
    {
        // see if the given EEPROM path is in the system configuration JSON
        // if yes, return the system configuration JSON
        if (systemConfigJson["frus"].contains(*vpdPath))
        {
            return std::cref(systemConfigJson);
        }

        return std::unexpected(error_code::PATH_NOT_FOUND_IN_JSON);
    }

    if (const auto itr = chassisIdToJsonMap.find(chassisId);
        itr != chassisIdToJsonMap.end())
    {
        return std::cref(itr->second);
    }

    return std::unexpected(error_code::PATH_NOT_FOUND_IN_JSON);
}

std::string ConfigManager::getChassisId(
    const std::string& inventoryObjPath) const noexcept
{
    try
    {
        auto startPos = inventoryObjPath.find("/chassis");
        if (std::string::npos == startPos)
        {
            return std::string{};
        }

        ++startPos;
        const auto endPos = inventoryObjPath.find('/', startPos);
        const auto chassisId =
            inventoryObjPath.substr(startPos, endPos - startPos);

        return chassisId;
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(std::format(
            "Failed to extract chassis ID from given path {}, error: {}",
            inventoryObjPath, ex.what()));
        return std::string{};
    }
}

void ConfigManager::buildConfigMaps()
{
    if (!systemConfigJson.contains("frus"))
    {
        throw JsonException{"System config JSON is invalid"};
    }

    // Iterate through "frus" under system config JSON
    const nlohmann::json& listOfFrus =
        systemConfigJson["frus"].get_ref<const nlohmann::json::object_t&>();

    // build JSON object which is common across all chassis JSONs
    //  Check if commonInterfaces exists in system config JSON and add to the
    //  entry if yes
    std::optional<nlohmann::json> commonJsonObj{std::nullopt};
    if (systemConfigJson.contains("commonInterfaces"))
    {
        // system config JSON has "commonInterfaces" so add it to the common
        // JSON object
        commonJsonObj.emplace(nlohmann::json::object(
            {{"commonInterfaces", systemConfigJson["commonInterfaces"]}}));
    }

    // Build maps for each FRU sequentially
    for (const auto& fruJsonObj : listOfFrus.items())
    {
        const auto mapBuildResult =
            buildConfigMapsForFru(fruJsonObj, commonJsonObj);

        if (!mapBuildResult.has_value())
        {
            // Log the error but continue processing other FRUs
            logger->logMessage(std::format(
                "Failed to build map for FRU {}. Error code: {}",
                fruJsonObj.key(),
                commonUtility::getErrCodeMsg(mapBuildResult.error())));
        }
    }
}

std::expected<bool, error_code> ConfigManager::buildConfigMapsForFru(
    const auto& fruJsonObj,
    const std::optional<nlohmann::json>& commonJsonObj) noexcept
{
    try
    {
        const auto& eepromPath = fruJsonObj.key();
        const auto& subFruJsonArray = fruJsonObj.value();

        // Validate FRU JSON is an array with at least one element
        if (!subFruJsonArray.is_array() || subFruJsonArray.empty())
        {
            return std::unexpected(error_code::INVALID_JSON);
        }

        // Extract chassis ID from inventory path of base FRU at index 0
        const auto baseInvObjPath =
            subFruJsonArray.at(0).value("inventoryPath", "");
        if (baseInvObjPath.empty())
        {
            return std::unexpected(error_code::INVALID_JSON);
        }

        const auto& chassisId = getChassisId(baseInvObjPath);
        if (chassisId.empty())
        {
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }

        // Create entry in EEPROM to chassis ID map
        eepromToChassisIdMap.emplace(eepromPath, chassisId);

        // Create entry in chassis to motherboard EEPROM map
        if (baseInvObjPath.ends_with("motherboard"))
        {
            chassisToMotherboardEepromMap.emplace(chassisId, eepromPath);
        }

        // Get or create chassis JSON object
        auto& chassisJson = chassisIdToJsonMap[chassisId];

        // check if chassis JSON has a "frus" section, if not create
        if (!chassisJson.contains("frus"))
        {
            chassisJson["frus"] = nlohmann::json::object();

            // check if there is any common JSON object to be appended to the
            // chassis JSON
            if (commonJsonObj)
            {
                chassisJson.update(commonJsonObj.value());
            }
        }

        // append the sub FRUs
        chassisJson["frus"][eepromPath] = subFruJsonArray;

        // build unexpanded location code to inventory path map
        return buildLocCodeToInvPathsMap(subFruJsonArray, chassisId);
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(
            std::format("Failed to build maps for FRU {}. Error: {}",
                        fruJsonObj.key(), ex.what()));
        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

std::expected<bool, error_code> ConfigManager::buildLocCodeToInvPathsMap(
    const auto& subFruJsonArray, const std::string& chassisId) noexcept
{
    try
    {
        const auto nodeIdResult =
            vpdSpecificUtility::getNodeIdentifierFromChassisId(chassisId);

        if (!nodeIdResult.has_value())
        {
            return std::unexpected(nodeIdResult.error());
        }

        const std::string& nodeId = nodeIdResult.value();

        for (const auto& subFruJson : subFruJsonArray)
        {
            // get the inventory path
            if (subFruJson.contains("inventoryPath"))
            {
                const auto& inventoryPath = subFruJson["inventoryPath"];

                // get the unexpanded location code
                const auto locationCode =
                    getUnexpandedLocationCodeForFru(subFruJson);
                if (locationCode.has_value())
                {
                    // Only insert the node identifier if the location code is
                    // long enough to contain a valid prefix (e.g. "Ufcs").
                    if (locationCode.value().length() >=
                        constants::UNEXP_LOCATION_CODE_MIN_LENGTH)
                    {
                        std::string mapKey = locationCode.value();
                        const auto dashPos = mapKey.find('-');
                        if (dashPos != std::string::npos)
                        {
                            mapKey.insert(dashPos + 1, nodeId + "-");
                        }
                        else
                        {
                            mapKey.append("-" + nodeId);
                        }
                        unexpandedLocCodeToInvPathsMap[mapKey] =
                            sdbusplus::object_path{std::string{inventoryPath}};
                    }
                }
                else
                {
                    logger->logMessage(std::format(
                        "Failed to get unexpanded location code for {}. Error: {}",
                        std::string{inventoryPath},
                        commonUtility::getErrCodeMsg(locationCode.error())));
                }
            }
        }
        return true;
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(std::format(
            "Failed to build location code to inventory path(s) map for FRU {}. Error: {}",
            subFruJsonArray[0].value("inventoryPath", ""), ex.what()));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

std::expected<sdbusplus::object_path, error_code>
    ConfigManager::getInventoryPath(
        const std::string& unexpandedLocationCode) const noexcept
{
    try
    {
        if (auto it =
                unexpandedLocCodeToInvPathsMap.find(unexpandedLocationCode);
            it != unexpandedLocCodeToInvPathsMap.end())
        {
            return it->second;
        }
        return std::unexpected(error_code::FRU_PATH_NOT_FOUND);
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(std::format(
            "Failed to get inventory path for unexpanded location code {}. Error: {}",
            unexpandedLocationCode, ex.what()));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

nlohmann::json ConfigManager::getParsedJson(const std::string& jsonPath,
                                            uint16_t& errCode) noexcept
{
    errCode = 0;

    if (jsonPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return nlohmann::json{};
    }

    if (!std::filesystem::exists(jsonPath))
    {
        errCode = error_code::FILE_NOT_FOUND;
        return nlohmann::json{};
    }

    if (std::filesystem::is_empty(jsonPath))
    {
        errCode = error_code::EMPTY_FILE;
        return nlohmann::json{};
    }

    std::ifstream jsonFile(jsonPath);
    if (!jsonFile)
    {
        errCode = error_code::FILE_ACCESS_ERROR;
        return nlohmann::json{};
    }

    try
    {
        return nlohmann::json::parse(jsonFile);
    }
    catch (const std::exception&)
    {
        errCode = error_code::JSON_PARSE_ERROR;
        return nlohmann::json{};
    }
}

std::expected<std::string, error_code>
    ConfigManager::getUnexpandedLocationCodeForFru(
        const nlohmann::json& fruJsonObj) noexcept
{
    try
    {
        if (!fruJsonObj.contains("extraInterfaces"))
        {
            return std::unexpected(error_code::MISSING_FLAG);
        }

        // look for extraInterfaces object
        const auto& extraInterfacesObj = fruJsonObj["extraInterfaces"];

        if (!extraInterfacesObj.contains(constants::locationCodeInf))
        {
            return std::unexpected(error_code::MISSING_FLAG);
        }

        const auto& locationCodeInfEntry =
            extraInterfacesObj[constants::locationCodeInf];

        if (!locationCodeInfEntry.contains("LocationCode"))
        {
            return std::unexpected(error_code::MISSING_FLAG);
        }

        return locationCodeInfEntry["LocationCode"];
    }
    catch (const std::exception& ex)
    {
        const std::string inventoryPath{fruJsonObj.value("inventoryPath", "")};

        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to get unexpanded location code for FRU: {}. Error: {}",
            inventoryPath, ex.what()));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

void ConfigManager::validateChassisSpecificJsons() noexcept
{
    // Validate all chassis-specific JSONs. if
    // any invalid JSON is found, remove the corresponding entry from the
    // chassis ID to chassis-specific JSON map
    std::erase_if(chassisIdToJsonMap, [this](const auto& mapEntry) {
        try
        {
            ConfigManager::JsonValidator::validateConfigJson(mapEntry.second);
        }
        catch (const std::exception& ex)
        {
            logger->logMessage(
                std::format(
                    "Failed to validate chassis config JSON for {}. Error: {}",
                    mapEntry.first, ex.what()),
                PlaceHolder::ASYNC_PEL,
                types::PelInfoTuple{types::ErrorType::FirmwareError,
                                    types::SeverityType::Warning, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt, std::nullopt});

            return true; // Validation failed. Erase this entry
        }
        return false;    // Keep this entry
    });
}

void ConfigManager::JsonValidator::validateConfigJson(
    const nlohmann::json& jsonObj)
{
    // Check if "frus" section exists (mandatory)
    if (!jsonObj.contains("frus"))
    {
        throw JsonException{
            "JSON validation failed: Missing required 'frus' section"};
    }

    // Check if "frus" is an object
    if (!jsonObj["frus"].is_object())
    {
        throw JsonException{
            "JSON validation failed: 'frus' section must be an object"};
    }

    // Check if "frus" is not empty
    if (jsonObj["frus"].empty())
    {
        throw JsonException{
            "JSON validation failed: 'frus' section cannot be empty"};
    }

    // Validate each FRU entry in "frus"
    const auto& frus = jsonObj["frus"];
    for (const auto& [eepromPath, fruArray] : frus.items())
    {
        // Check if FRU value is an array
        if (!fruArray.is_array())
        {
            throw JsonException{
                std::format("JSON validation failed: FRU '{}' must be an array",
                            eepromPath)};
        }

        // Check if FRU array is not empty
        if (fruArray.empty())
        {
            throw JsonException{std::format(
                "JSON validation failed: FRU '{}' array cannot be empty",
                eepromPath)};
        }

        // Validate each sub-FRU in the array
        for (size_t i = 0; i < fruArray.size(); ++i)
        {
            const auto& subFru = fruArray[i];

            // Check if sub-FRU is an object
            if (!subFru.is_object())
            {
                throw JsonException{std::format(
                    "JSON validation failed: Sub-FRU at index {} in '{}' must be an object",
                    i, eepromPath)};
            }

            // Validate sub-FRU structure
            validateSubFruJson(subFru, eepromPath, i);
        }
    }
}

void ConfigManager::JsonValidator::validateSubFruJson(
    const nlohmann::json& subFruJson, const std::string& eepromPath,
    const size_t index)
{
    // Validate mandatory tags
    validateMandatoryTags(subFruJson, eepromPath, index);

    // Validate optional tags
    validateOptionalTags(subFruJson, eepromPath, index);
}

void ConfigManager::JsonValidator::validateMandatoryTags(
    const nlohmann::json& subFruJson, const std::string& eepromPath,
    const size_t index)
{
    // Check for mandatory field: inventoryPath
    if (!subFruJson.contains("inventoryPath"))
    {
        throw JsonException{std::format(
            "JSON validation failed: Sub-FRU at index {} in '{}' missing required 'inventoryPath' field",
            index, eepromPath)};
    }

    // Validate inventoryPath is a string
    if (!subFruJson["inventoryPath"].is_string())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'inventoryPath' in sub-FRU at index {} in '{}' must be a string",
            index, eepromPath)};
    }

    // Check for mandatory field: serviceName
    if (!subFruJson.contains("serviceName"))
    {
        throw JsonException{std::format(
            "JSON validation failed: Sub-FRU at index {} in '{}' missing required 'serviceName' field",
            index, eepromPath)};
    }

    // Validate serviceName is a string
    if (!subFruJson["serviceName"].is_string())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'serviceName' in sub-FRU at index {} in '{}' must be a string",
            index, eepromPath)};
    }
}

void ConfigManager::JsonValidator::validateOptionalTags(
    const nlohmann::json& subFruJson, const std::string& eepromPath,
    const size_t index)
{
    // Validate optional field: extraInterfaces (if present, must be an object)
    if (subFruJson.contains("extraInterfaces") &&
        !subFruJson["extraInterfaces"].is_object())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'extraInterfaces' in sub-FRU at index {} in '{}' must be an object",
            index, eepromPath)};
    }

    // If "preAction" exists, validate it's an object
    if (subFruJson.contains("preAction") &&
        !subFruJson["preAction"].is_object())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'preAction' in sub-FRU at index {} in '{}' must be an object",
            index, eepromPath)};
    }

    // If "postAction" exists, validate it's an object
    if (subFruJson.contains("postAction") &&
        !subFruJson["postAction"].is_object())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'postAction' in sub-FRU at index {} in '{}' must be an object",
            index, eepromPath)};
    }

    // If "postFailAction" exists, validate it's an object
    if (subFruJson.contains("postFailAction") &&
        !subFruJson["postFailAction"].is_object())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'postFailAction' in sub-FRU at index {} in '{}' must be an object",
            index, eepromPath)};
    }

    // If "pollingRequired" exists, validate it's an object and validate
    // "hotPlugging" nested within it
    if (subFruJson.contains("pollingRequired"))
    {
        if (!subFruJson["pollingRequired"].is_object())
        {
            throw JsonException{std::format(
                "JSON validation failed: 'pollingRequired' in sub-FRU at index {} in '{}' must be an object",
                index, eepromPath)};
        }

        validatePollingRequiredTag(subFruJson["pollingRequired"], eepromPath,
                                   index);
    }

    // If "copyRecords" exists, validate it's an array
    if (subFruJson.contains("copyRecords") &&
        !subFruJson["copyRecords"].is_array())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'copyRecords' in sub-FRU at index {} in '{}' must be an array",
            index, eepromPath)};
    }

    // If boolean fields exist, validate they are boolean type
    const std::vector<std::string> boolFields = {
        "isSystemVpd",
        "replaceableAtStandby",
        "replaceableAtRuntime",
        "concurrentlyMaintainable",
        "powerOffOnly",
        "embedded",
        "synthesized",
        "noprime",
        "handlePresence",
        "monitorPresence",
        "essentialFru",
        "readOnly",
        "inherit"};

    for (const auto& field : boolFields)
    {
        if (subFruJson.contains(field) && !subFruJson[field].is_boolean())
        {
            throw JsonException{std::format(
                "JSON validation failed: '{}' in sub-FRU at index {} in '{}' must be a boolean",
                field, index, eepromPath)};
        }
    }

    // If string fields exist, validate they are string type
    const std::vector<std::string> stringFields = {
        "redundantEeprom", "cpuType", "busType", "driverType", "devAddress"};

    for (const auto& field : stringFields)
    {
        if (subFruJson.contains(field) && !subFruJson[field].is_string())
        {
            throw JsonException{std::format(
                "JSON validation failed: '{}' in sub-FRU at index {} in '{}' must be a string",
                field, index, eepromPath)};
        }
    }

    // If integer fields exist, validate they are number type
    const std::vector<std::string> intFields = {"offset", "size"};

    for (const auto& field : intFields)
    {
        if (subFruJson.contains(field) &&
            !subFruJson[field].is_number_integer())
        {
            throw JsonException{std::format(
                "JSON validation failed: '{}' in sub-FRU at index {} in '{}' must be an integer",
                field, index, eepromPath)};
        }
    }
}

void ConfigManager::JsonValidator::validatePollingRequiredTag(
    const nlohmann::json& pollingRequiredJson, const std::string& eepromPath,
    const size_t index)
{
    if (pollingRequiredJson.contains("hotPlugging"))
    {
        const auto& hotPlugging = pollingRequiredJson["hotPlugging"];

        if (!hotPlugging.is_object())
        {
            throw JsonException{std::format(
                "JSON validation failed: 'hotPlugging' in 'pollingRequired' in sub-FRU at index {} in '{}' must be an object",
                index, eepromPath)};
        }

        if (hotPlugging.contains("gpioPresence"))
        {
            const auto& gpioPresence = hotPlugging["gpioPresence"];

            if (!gpioPresence.is_object())
            {
                throw JsonException{std::format(
                    "JSON validation failed: 'gpioPresence' in 'hotPlugging' in sub-FRU at index {} in '{}' must be an object",
                    index, eepromPath)};
            }

            // check for "pin" tag
            if (!gpioPresence.contains("pin"))
            {
                throw JsonException{std::format(
                    "JSON validation failed: 'pin' tag missing in 'gpioPresence' in sub-FRU at index {} in '{}'",
                    index, eepromPath)};
            }

            if (!gpioPresence["pin"].is_string())
            {
                throw JsonException{std::format(
                    "JSON validation failed: 'pin' in 'gpioPresence' in sub-FRU at index {} in '{}' must be a string",
                    index, eepromPath)};
            }

            // check for "value" tag
            if (!gpioPresence.contains("value"))
            {
                throw JsonException{std::format(
                    "JSON validation failed: 'value' tag missing in 'gpioPresence' in sub-FRU at index {} in '{}'",
                    index, eepromPath)};
            }

            if (!gpioPresence["value"].is_number_integer())
            {
                throw JsonException{std::format(
                    "JSON validation failed: 'value' in 'gpioPresence' in sub-FRU at index {} in '{}' must be an integer",
                    index, eepromPath)};
            }
        }
    }
}

bool ConfigManager::isInventoryPathInJson(
    const std::string& invPath) const noexcept
{
    // Validate: must be a non-empty D-Bus inventory object path.
    // EEPROM paths (filesystem paths) do not start with pimPath, so this
    // check also naturally rejects them.
    if (invPath.empty() || !invPath.starts_with(constants::pimPath))
    {
        return false;
    }

    try
    {
        for (const auto& fruEntry : systemConfigJson["frus"].items())
        {
            const auto& subFruJsonArray = fruEntry.value();

            const auto findResult = std::ranges::find_if(
                subFruJsonArray, [&invPath](const nlohmann::json& subFruJson) {
                    return subFruJson.value("inventoryPath", "") == invPath;
                });

            if (findResult != subFruJsonArray.end())
            {
                return true;
            }
        }
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(
            std::format("Failed to check if path {} is in JSON, error: {}",
                        invPath, ex.what()));
    }

    return false;
}
} // namespace vpd
