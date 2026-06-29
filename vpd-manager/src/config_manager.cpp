#include "config_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "utility/common_utility.hpp"

#include <format>

namespace vpd
{

// Static singleton instance — null until initialize() is called
std::atomic<std::shared_ptr<ConfigManager>> ConfigManager::m_instance{nullptr};

std::shared_ptr<ConfigManager> ConfigManager::getInstance() noexcept
{
    return m_instance.load();
}

std::shared_ptr<ConfigManager> ConfigManager::initialize(
    [[maybe_unused]] const ManagerPassKey& i_key,
    const std::string& i_sysConfigJsonPath)
{
    // Build the new instance entirely on the side so that any concurrent
    // reader holding a snapshot of m_instance continues to see the old,
    // fully-populated data right up until the atomic pointer swap below.
    // This eliminates the empty-JSON window that the old clear()+loadJson()
    // sequence inside reinitialize() introduced (Con 2), and removes the
    // ordering precondition that reinitialize() imposed (Con 3).
    auto l_newInstance = std::shared_ptr<ConfigManager>(new ConfigManager());
    l_newInstance->loadJson(i_sysConfigJsonPath);

    // Flag is true when Phase 1 default JSON is loaded; false after Phase 2
    // loads the system-specific symlinked JSON.
    l_newInstance->m_isDefaultJson =
        (i_sysConfigJsonPath == INVENTORY_JSON_DEFAULT);

    // Single atomic pointer swap — readers see either the old complete
    // state or the new complete state, never an intermediate state.
    m_instance.store(l_newInstance);
    return m_instance.load();
}

void ConfigManager::loadJson(const std::string& i_sysConfigJsonPath)
{
    uint16_t l_errCode{constants::VALUE_0};

    m_systemConfigJson = getParsedJson(i_sysConfigJsonPath, l_errCode);

    if (l_errCode != constants::VALUE_0)
    {
        throw JsonException{std::format(
            "ConfigManager failed to load JSON from path {}. Error: {}",
            i_sysConfigJsonPath, commonUtility::getErrCodeMsg(l_errCode))};
    }

    // Validate the system configuration JSON
    JsonValidator::validateConfigJson(m_systemConfigJson);

    buildConfigMaps();

    // Validate the chassis-specific JSONs
    validateChassisSpecificJsons();
}

const nlohmann::json& ConfigManager::getJsonObj(
    const std::optional<std::string>& i_vpdPath) const noexcept
{
    if (!i_vpdPath)
    {
        return m_systemConfigJson;
    }

    std::string l_chassisId{};

    if ((*i_vpdPath).starts_with(std::string_view(constants::pimPath)))
    {
        l_chassisId = getChassisId(*i_vpdPath);
    }
    else if (const auto l_itr = m_eepromToChassisIdMap.find(*i_vpdPath);
             l_itr != m_eepromToChassisIdMap.end())
    {
        l_chassisId = l_itr->second;
    }
    else
    {
        m_logger->logMessage(std::format(
            "Invalid input path {}, received to get chasiss specific JSON object.",
            *i_vpdPath));
        return m_systemConfigJson;
    }

    if (const auto l_itr = m_chassisIdToJsonMap.find(l_chassisId);
        l_itr != m_chassisIdToJsonMap.end())
    {
        return l_itr->second;
    }

    return m_systemConfigJson;
}

std::string ConfigManager::getChassisId(
    const std::string& i_inventoryObjPath) const noexcept
{
    try
    {
        auto l_startPos = i_inventoryObjPath.find("/chassis");
        if (std::string::npos == l_startPos)
        {
            return std::string{};
        }

        ++l_startPos;
        const auto l_endPos = i_inventoryObjPath.find('/', l_startPos);
        const auto l_chassisId =
            i_inventoryObjPath.substr(l_startPos, l_endPos - l_startPos);

        return l_chassisId;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to extract chassis ID from given path {}, error: {}",
            i_inventoryObjPath, l_ex.what()));
        return std::string{};
    }
}

void ConfigManager::buildConfigMaps()
{
    if (!m_systemConfigJson.contains("frus"))
    {
        throw JsonException{"System config JSON is invalid"};
    }

    // Iterate through "frus" under system config JSON
    const nlohmann::json& l_listOfFrus =
        m_systemConfigJson["frus"].get_ref<const nlohmann::json::object_t&>();

    // build JSON object which is common across all chassis JSONs
    //  Check if commonInterfaces exists in system config JSON and add to the
    //  entry if yes
    std::optional<nlohmann::json> l_commonJsonObj{std::nullopt};
    if (m_systemConfigJson.contains("commonInterfaces"))
    {
        // system config JSON has "commonInterfaces" so add it to the common
        // JSON object
        l_commonJsonObj.emplace(nlohmann::json::object(
            {{"commonInterfaces", m_systemConfigJson["commonInterfaces"]}}));
    }

    // Build maps for each FRU sequentially
    for (const auto& l_fruJsonObj : l_listOfFrus.items())
    {
        const auto l_mapBuildResult =
            buildConfigMapsForFru(l_fruJsonObj, l_commonJsonObj);

        if (!l_mapBuildResult.has_value())
        {
            // Log the error but continue processing other FRUs
            m_logger->logMessage(std::format(
                "Failed to build map for FRU {}. Error code: {}",
                l_fruJsonObj.key(),
                commonUtility::getErrCodeMsg(l_mapBuildResult.error())));
        }
    }
}

std::expected<bool, error_code> ConfigManager::buildConfigMapsForFru(
    const auto& i_fruJsonObj,
    const std::optional<nlohmann::json>& i_commonJsonObj) noexcept
{
    try
    {
        const auto& l_eepromPath = i_fruJsonObj.key();
        const auto& l_subFruJsonArray = i_fruJsonObj.value();

        // Validate FRU JSON is an array with at least one element
        if (!l_subFruJsonArray.is_array() || l_subFruJsonArray.empty())
        {
            return std::unexpected(error_code::INVALID_JSON);
        }

        // Extract chassis ID from inventory path of base FRU at index 0
        const auto l_baseInvObjPath =
            l_subFruJsonArray.at(0).value("inventoryPath", "");
        if (l_baseInvObjPath.empty())
        {
            return std::unexpected(error_code::INVALID_JSON);
        }

        // @todo: handle inventory paths without chassis information once they
        // are modelled in system config JSON
        const auto& l_chassisId = getChassisId(l_baseInvObjPath);
        if (l_chassisId.empty())
        {
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }

        // Create entry in EEPROM to chassis ID map
        m_eepromToChassisIdMap.emplace(l_eepromPath, l_chassisId);

        // Create entry in chassis to motherboard EEPROM map
        if (l_baseInvObjPath.ends_with("motherboard"))
        {
            m_chassisToMotherboardEepromMap.emplace(l_chassisId, l_eepromPath);
        }

        // Get or create chassis JSON object
        auto& l_chassisJson = m_chassisIdToJsonMap[l_chassisId];

        // check if chassis JSON has a "frus" section, if not create
        if (!l_chassisJson.contains("frus"))
        {
            l_chassisJson["frus"] = nlohmann::json::object();

            // check if there is any common JSON object to be appended to the
            // chassis JSON
            if (i_commonJsonObj)
            {
                l_chassisJson.update(i_commonJsonObj.value());
            }
        }

        // append the sub FRUs
        l_chassisJson["frus"][l_eepromPath] = l_subFruJsonArray;

        // build unexpanded location code to inventory path map
        return buildLocCodeToInvPathsMap(l_subFruJsonArray);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Failed to build maps for FRU {}. Error: {}",
                        i_fruJsonObj.key(), l_ex.what()));
        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

std::expected<bool, error_code> ConfigManager::buildLocCodeToInvPathsMap(
    const auto& i_subFruJsonArray) noexcept
{
    try
    {
        for (const auto& l_subFruJson : i_subFruJsonArray)
        {
            // get the inventory path
            if (l_subFruJson.contains("inventoryPath"))
            {
                const auto& l_inventoryPath = l_subFruJson["inventoryPath"];

                // get the unexpanded location code
                const auto l_locationCode =
                    getUnexpandedLocationCodeForFru(l_subFruJson);
                if (l_locationCode.has_value())
                {
                    m_unexpandedLocCodeToInvPathsMap[l_locationCode.value()]
                        .emplace_back(l_inventoryPath);
                }
                else
                {
                    m_logger->logMessage(std::format(
                        "Failed to get unexpanded location code for {}. Error: {}",
                        std::string{l_inventoryPath},
                        commonUtility::getErrCodeMsg(l_locationCode.error())));
                }
            }
        }
        return true;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to build location code to inventory path(s) map for FRU {}. Error: {}",
            i_subFruJsonArray[0].value("inventoryPath", ""), l_ex.what()));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

std::expected<types::ListOfPaths, error_code> ConfigManager::getInventoryPaths(
    const std::string& i_unexpandedLocationCode) const noexcept
{
    try
    {
        if (auto l_it =
                m_unexpandedLocCodeToInvPathsMap.find(i_unexpandedLocationCode);
            l_it != m_unexpandedLocCodeToInvPathsMap.end())
        {
            return l_it->second;
        }
        return std::unexpected(error_code::FRU_PATH_NOT_FOUND);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to get inventory paths for unexpanded location code {}. Error: {}",
            i_unexpandedLocationCode, l_ex.what()));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

nlohmann::json ConfigManager::getParsedJson(const std::string& i_jsonPath,
                                            uint16_t& o_errCode) noexcept
{
    o_errCode = 0;

    if (i_jsonPath.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return nlohmann::json{};
    }

    if (!std::filesystem::exists(i_jsonPath))
    {
        o_errCode = error_code::FILE_NOT_FOUND;
        return nlohmann::json{};
    }

    if (std::filesystem::is_empty(i_jsonPath))
    {
        o_errCode = error_code::EMPTY_FILE;
        return nlohmann::json{};
    }

    std::ifstream l_jsonFile(i_jsonPath);
    if (!l_jsonFile)
    {
        o_errCode = error_code::FILE_ACCESS_ERROR;
        return nlohmann::json{};
    }

    try
    {
        return nlohmann::json::parse(l_jsonFile);
    }
    catch (const std::exception&)
    {
        o_errCode = error_code::JSON_PARSE_ERROR;
        return nlohmann::json{};
    }
}

std::expected<std::string, error_code>
    ConfigManager::getUnexpandedLocationCodeForFru(
        const nlohmann::json& i_fruJsonObj) noexcept
{
    try
    {
        if (!i_fruJsonObj.contains("extraInterfaces"))
        {
            return std::unexpected(error_code::MISSING_FLAG);
        }

        // look for extraInterfaces object
        const auto& l_extraInterfacesObj = i_fruJsonObj["extraInterfaces"];

        if (!l_extraInterfacesObj.contains(constants::locationCodeInf))
        {
            return std::unexpected(error_code::MISSING_FLAG);
        }

        const auto& l_locationCodeInfEntry =
            l_extraInterfacesObj[constants::locationCodeInf];

        if (!l_locationCodeInfEntry.contains("LocationCode"))
        {
            return std::unexpected(error_code::MISSING_FLAG);
        }

        return l_locationCodeInfEntry["LocationCode"];
    }
    catch (const std::exception& l_ex)
    {
        const std::string l_inventoryPath{
            i_fruJsonObj.value("inventoryPath", "")};

        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to get unexpanded location code for FRU: {}. Error: {}",
            l_inventoryPath, l_ex.what()));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

void ConfigManager::validateChassisSpecificJsons() const noexcept
{
    // iterate through all chassis-specific JSONs and validate each of them
    for (const auto& [l_chassisId, l_chassisJsonObj] : m_chassisIdToJsonMap)
    {
        try
        {
            ConfigManager::JsonValidator::validateConfigJson(l_chassisJsonObj);
        }
        catch (const JsonException& l_ex)
        {
            m_logger->logMessage(
                std::format("Chassis {} JSON validation failed. Error: {}",
                            l_chassisId, l_ex.what()));

            // TODO: decide if the chassis JSON object should be marked as
            // invalid in the map
        }
    }
}

void ConfigManager::JsonValidator::validateConfigJson(
    const nlohmann::json& i_jsonObj)
{
    // Check if "frus" section exists (mandatory)
    if (!i_jsonObj.contains("frus"))
    {
        throw JsonException{
            "JSON validation failed: Missing required 'frus' section"};
    }

    // Check if "frus" is an object
    if (!i_jsonObj["frus"].is_object())
    {
        throw JsonException{
            "JSON validation failed: 'frus' section must be an object"};
    }

    // Check if "frus" is not empty
    if (i_jsonObj["frus"].empty())
    {
        throw JsonException{
            "JSON validation failed: 'frus' section cannot be empty"};
    }

    // Validate each FRU entry in "frus"
    const auto& l_frus = i_jsonObj["frus"];
    for (const auto& [l_eepromPath, l_fruArray] : l_frus.items())
    {
        // Check if FRU value is an array
        if (!l_fruArray.is_array())
        {
            throw JsonException{
                std::format("JSON validation failed: FRU '{}' must be an array",
                            l_eepromPath)};
        }

        // Check if FRU array is not empty
        if (l_fruArray.empty())
        {
            throw JsonException{std::format(
                "JSON validation failed: FRU '{}' array cannot be empty",
                l_eepromPath)};
        }

        // Validate each sub-FRU in the array
        for (size_t i = 0; i < l_fruArray.size(); ++i)
        {
            const auto& l_subFru = l_fruArray[i];

            // Check if sub-FRU is an object
            if (!l_subFru.is_object())
            {
                throw JsonException{std::format(
                    "JSON validation failed: Sub-FRU at index {} in '{}' must be an object",
                    i, l_eepromPath)};
            }

            // Validate sub-FRU structure
            validateSubFruJson(l_subFru, l_eepromPath, i);
        }
    }
}

void ConfigManager::JsonValidator::validateSubFruJson(
    const nlohmann::json& i_subFruJson, const std::string& i_eepromPath,
    size_t i_index)
{
    // Validate mandatory tags
    validateMandatoryTags(i_subFruJson, i_eepromPath, i_index);

    // Validate optional tags
    validateOptionalTags(i_subFruJson, i_eepromPath, i_index);
}

void ConfigManager::JsonValidator::validateMandatoryTags(
    const nlohmann::json& i_subFruJson, const std::string& i_eepromPath,
    size_t i_index)
{
    // Check for mandatory field: inventoryPath
    if (!i_subFruJson.contains("inventoryPath"))
    {
        throw JsonException{std::format(
            "JSON validation failed: Sub-FRU at index {} in '{}' missing required 'inventoryPath' field",
            i_index, i_eepromPath)};
    }

    // Validate inventoryPath is a string
    if (!i_subFruJson["inventoryPath"].is_string())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'inventoryPath' in sub-FRU at index {} in '{}' must be a string",
            i_index, i_eepromPath)};
    }

    // Check for mandatory field: serviceName
    if (!i_subFruJson.contains("serviceName"))
    {
        throw JsonException{std::format(
            "JSON validation failed: Sub-FRU at index {} in '{}' missing required 'serviceName' field",
            i_index, i_eepromPath)};
    }

    // Validate serviceName is a string
    if (!i_subFruJson["serviceName"].is_string())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'serviceName' in sub-FRU at index {} in '{}' must be a string",
            i_index, i_eepromPath)};
    }
}

void ConfigManager::JsonValidator::validateOptionalTags(
    const nlohmann::json& i_subFruJson, const std::string& i_eepromPath,
    size_t i_index)
{
    // Validate optional field: extraInterfaces (if present, must be an object)
    if (i_subFruJson.contains("extraInterfaces") &&
        !i_subFruJson["extraInterfaces"].is_object())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'extraInterfaces' in sub-FRU at index {} in '{}' must be an object",
            i_index, i_eepromPath)};
    }

    // If "preAction" exists, validate it's an object
    if (i_subFruJson.contains("preAction") &&
        !i_subFruJson["preAction"].is_object())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'preAction' in sub-FRU at index {} in '{}' must be an object",
            i_index, i_eepromPath)};
    }

    // If "postAction" exists, validate it's an object
    if (i_subFruJson.contains("postAction") &&
        !i_subFruJson["postAction"].is_object())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'postAction' in sub-FRU at index {} in '{}' must be an object",
            i_index, i_eepromPath)};
    }

    // If "postFailAction" exists, validate it's an object
    if (i_subFruJson.contains("postFailAction") &&
        !i_subFruJson["postFailAction"].is_object())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'postFailAction' in sub-FRU at index {} in '{}' must be an object",
            i_index, i_eepromPath)};
    }

    // If "copyRecords" exists, validate it's an array
    if (i_subFruJson.contains("copyRecords") &&
        !i_subFruJson["copyRecords"].is_array())
    {
        throw JsonException{std::format(
            "JSON validation failed: 'copyRecords' in sub-FRU at index {} in '{}' must be an array",
            i_index, i_eepromPath)};
    }

    // If boolean fields exist, validate they are boolean type
    const std::vector<std::string> l_boolFields = {
        "isSystemVpd",
        "replaceableAtStandby",
        "replaceableAtRuntime",
        "pollingRequired",
        "hotPlugging",
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

    for (const auto& l_field : l_boolFields)
    {
        if (i_subFruJson.contains(l_field) &&
            !i_subFruJson[l_field].is_boolean())
        {
            throw JsonException{std::format(
                "JSON validation failed: '{}' in sub-FRU at index {} in '{}' must be a boolean",
                l_field, i_index, i_eepromPath)};
        }
    }

    // If string fields exist, validate they are string type
    const std::vector<std::string> l_stringFields = {
        "redundantEeprom", "cpuType", "busType", "driverType", "devAddress"};

    for (const auto& l_field : l_stringFields)
    {
        if (i_subFruJson.contains(l_field) &&
            !i_subFruJson[l_field].is_string())
        {
            throw JsonException{std::format(
                "JSON validation failed: '{}' in sub-FRU at index {} in '{}' must be a string",
                l_field, i_index, i_eepromPath)};
        }
    }

    // If integer fields exist, validate they are number type
    const std::vector<std::string> l_intFields = {"offset", "size"};

    for (const auto& l_field : l_intFields)
    {
        if (i_subFruJson.contains(l_field) &&
            !i_subFruJson[l_field].is_number_integer())
        {
            throw JsonException{std::format(
                "JSON validation failed: '{}' in sub-FRU at index {} in '{}' must be an integer",
                l_field, i_index, i_eepromPath)};
        }
    }
}
} // namespace vpd
