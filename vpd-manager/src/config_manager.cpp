#include "config_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"

#include <format>

namespace vpd
{
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
        const auto l_mapBuildResult = buildMapForFru(l_eepromPath, l_fruJson);
        if (!l_mapBuildResult.has_value())
        {
            // Log the error but continue processing other FRUs
            m_logger->logMessage(std::format(
                "Failed to build map for FRU {}. Error code: {}", l_eepromPath,
                static_cast<int>(l_mapBuildResult.error())));
        }
    }
}

std::expected<std::string_view, error_code>
    ConfigManager::getChassisIdFromObjectPath(
        [[maybe_unused]] const std::string_view i_objPath) const noexcept
{
    try
    {
        std::string_view l_chassisId;

        /* TODO
            - extract chassis ID string from the object path
        */
        return l_chassisId;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to extract chassis ID from object path {}. Error: {}",
            i_objPath, l_ex.what()));
    }
    return std::unexpected(error_code::STANDARD_EXCEPTION);
}

std::expected<bool, error_code> ConfigManager::buildMapForFru(
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
} // namespace vpd
