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

void ConfigManager::buildChassisToFruMap()
{
    if (!m_systemConfigJson.contains("frus"))
    {
        throw JsonException{"System config JSON is invalid"};
    }

    // Iterate through "frus" under system config JSON
    const nlohmann::json& l_listOfFrus =
        m_systemConfigJson["frus"].get_ref<const nlohmann::json::object_t&>();

    // Build maps for each FRU sequentially
    for (const auto& [l_eepromPath, l_fruJson] : l_listOfFrus.items())
    {
        const auto l_mapBuildResult = buildMapsForFru(l_eepromPath, l_fruJson);
        if (!l_mapBuildResult.has_value())
        {
            // Log the error but continue processing other FRUs
            m_logger->logMessage(std::format(
                "Failed to build map for FRU {}. Error code: {}", l_eepromPath,
                commonUtility::getErrCodeMsg(l_mapBuildResult.error())));
        }
    }
}

std::expected<bool, error_code> ConfigManager::buildMapsForFru(
    const std::string& i_eepromPath,
    [[maybe_unused]] const nlohmann::json& i_fruJson) noexcept
{
    try
    {
        bool l_rc{true};
        /*  TODO:
            - extract chassis ID from inventory path of base FRU at index 0
            - create entry in EEPROM to chassis ID map
            - create entry in chassis ID to JSON map
            - get commonInterfaces JSON object from system config JSON and add
           in the entry
            - iterate through all sub FRU JSON objects and append to chassis
           specific entry in chassis ID to JSON map
        */

        return l_rc;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Failed to build maps for FRU {}. Error: {}",
                        i_eepromPath, l_ex.what()));
        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

std::string ConfigManager::getChassisId(
    const std::string& i_inventoryObjPath) const noexcept
{
    auto l_startPos = i_inventoryObjPath.find("/chassis");
    if (std::string_view::npos == l_startPos)
    {
        return std::string{};
    }

    ++l_startPos;
    const auto l_endPos = i_inventoryObjPath.find('/', l_startPos);
    const std::string_view l_chassisId =
        i_inventoryObjPath.substr(l_startPos, l_endPos - l_startPos);

    return std::string(l_chassisId);
}
} // namespace vpd
