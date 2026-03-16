#include "config_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "utility/common_utility.hpp"

#include <format>
namespace vpd
{
const nlohmann::json& ConfigManager::getJsonObj(
    const std::optional<std::string>& i_vpdPath) const noexcept
{
    if (i_vpdPath && i_vpdPath.value().starts_with(constants::pimPath))
    {
        [[maybe_unused]] const auto l_chassisId =
            getChassisId(i_vpdPath.value());
    }

    /**
     * @todo Implement the following logic:
     *  - If @p i_vpdPath is an EEPROM path, obtain the chassisId from
     * m_eepromToChassisIdMap.
     *  - Return the chassis-specific JSON configuration from m_chassisInfoMap
     * using the resolved chassisId.
     *  - If @p i_vpdPath is std::nullopt, return m_systemConfigJson.
     */

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

void ConfigManager::buildChassisToFruMap()
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
    nlohmann::json l_commonJsonObj;
    if (!m_systemConfigJson.contains("commonInterfaces"))
    {
        m_logger->logMessage(
            "commonInterfaces not found in system config JSON");
    }
    else
    {
        // if the chassis JSON doesn't have "commonInterfaces", append it
        l_commonJsonObj["commonInterfaces"] =
            m_systemConfigJson["commonInterfaces"];
    }

    // Build maps for each FRU sequentially
    for (const auto& l_fruJsonObj : l_listOfFrus.items())
    {
        const auto l_mapBuildResult =
            l_commonJsonObj.empty()
                ? buildMapsForFru(l_fruJsonObj)
                : buildMapsForFru(l_fruJsonObj, l_commonJsonObj);

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

std::expected<bool, error_code> ConfigManager::buildMapsForFru(
    const auto& i_fruJsonObj,
    std::optional<nlohmann::json> i_commonJsonObj) noexcept
{
    try
    {
        bool l_rc{true};

        const auto& l_eepromPath = i_fruJsonObj.key();
        const auto& l_subFruJsonArray = i_fruJsonObj.value();

        // Validate FRU JSON is an array with at least one element
        if (!l_subFruJsonArray.is_array() || l_subFruJsonArray.empty())
        {
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        // Extract chassis ID from inventory path of base FRU at index 0
        const auto l_baseInvObjPath =
            l_subFruJsonArray.at(0).value("inventoryPath", "");
        if (l_baseInvObjPath.empty())
        {
            return std::unexpected(error_code::INVALID_JSON);
        }

        const auto& l_chassisId = getChassisId(l_baseInvObjPath);
        if (l_chassisId.empty())
        {
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }

        // Create entry in EEPROM to chassis ID map
        m_eepromToChassisIdMap.emplace(l_eepromPath, std::string(l_chassisId));

        // Get or create chassis JSON object
        auto& l_chassisJson = m_chassisIdToJsonMap[std::string(l_chassisId)];
        if (l_chassisJson.is_null())
        {
            l_chassisJson = nlohmann::json::object_t();
        }

        // check if chassis JSON has a "frus" section, if not create
        if (!l_chassisJson.contains("frus"))
        {
            l_chassisJson["frus"] = nlohmann::json::object();
        }

        // append the sub FRUs
        l_chassisJson["frus"][l_eepromPath] = l_subFruJsonArray;

        // check if there is any common JSON object to be appended to the
        // chassis JSON
        if (i_commonJsonObj)
        {
            l_chassisJson.update(i_commonJsonObj.value());
        }

        return l_rc;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Failed to build maps for FRU {}. Error: {}",
                        i_fruJsonObj.key(), l_ex.what()));
        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}
} // namespace vpd
