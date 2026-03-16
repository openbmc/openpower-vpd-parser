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
        const auto l_mapBuildResult = buildMapsForFru(l_eepromPath, l_fruJson);
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
        const std::string_view i_objPath) const noexcept
{
    try
    {
        if (i_objPath.empty())
        {
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        // Find system inventory path in the object path
        const auto l_systemInvPathPos =
            i_objPath.find(constants::systemInvPath);
        if (l_systemInvPathPos == std::string_view::npos)
        {
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }

        // Extract portion after system inventory path
        const auto l_afterSystemPath = i_objPath.substr(
            l_systemInvPathPos +
            std::string_view(constants::systemInvPath).length());

        if (l_afterSystemPath.empty() || l_afterSystemPath[0] != '/')
        {
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }

        // Skip leading '/' and find next '/' to extract chassis ID
        const auto l_pathAfterSlash = l_afterSystemPath.substr(1);
        const auto l_nextSlashPos = l_pathAfterSlash.find('/');

        std::string_view l_chassisId =
            (l_nextSlashPos != std::string_view::npos)
                ? l_pathAfterSlash.substr(0, l_nextSlashPos)
                : l_pathAfterSlash;

        if (l_chassisId.empty())
        {
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }

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

std::expected<bool, error_code> ConfigManager::buildMapsForFru(
    const std::string& i_eepromPath,
    [[maybe_unused]] const nlohmann::json& i_fruJson) noexcept
{
    try
    {
        bool l_rc{true};

        // Validate FRU JSON is an array with at least one element
        if (!i_fruJson.is_array() || i_fruJson.empty())
        {
            m_logger->logMessage(std::format(
                "FRU {} has no sub-FRUs or invalid format", i_eepromPath));
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        // Extract chassis ID from inventory path of base FRU at index 0
        const auto l_baseInvObjPath =
            i_fruJson.at(0).value("inventoryPath", "");
        if (l_baseInvObjPath.empty())
        {
            m_logger->logMessage(std::format(
                "Base object path is empty for FRU {}", i_eepromPath));
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        const auto l_chassisIdRes =
            getChassisIdFromObjectPath(l_baseInvObjPath);
        if (!l_chassisIdRes.has_value())
        {
            return std::unexpected(l_chassisIdRes.error());
        }

        const auto& l_chassisId = l_chassisIdRes.value();

        // Create entry in EEPROM to chassis ID map
        m_eepromToChassisIdMap.emplace(i_eepromPath, std::string(l_chassisId));

        /*  TODO:
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
