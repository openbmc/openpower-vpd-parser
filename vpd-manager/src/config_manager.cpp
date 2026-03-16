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

    // vector to hold result of map build for each FRU
    std::vector<std::future<std::expected<bool, error_code>>> l_mapBuildFutures;

    // launch async tasks to build maps for each FRU in the system config JSON
    for (const auto& l_aFru : l_listOfFrus.items())
    {
        l_mapBuildFutures.push_back(std::async(
            std::launch::async, &ConfigManager::buildMapForFru, this, l_aFru));
    }

    // process results and log any errors
    for (const auto& l_mapBuildFuture : l_mapBuildFutures)
    {
        const auto l_mapBuildResult = l_mapBuildFuture.get();
        if (!l_mapBuildResult.has_value())
        {
            // Log the error but continue processing other FRUs
            m_logger->logMessage(std::format(
                "Failed to build map for a FRU. Error code: {}",
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

        // Find the system inventory path in the object path
        const auto l_systemInvPathPos = i_objPath.find(constants::systemInvPath);
        if (l_systemInvPathPos == std::string_view::npos)
        {
            m_logger->logMessage(std::format(
                "System inventory path not found in object path: {}", i_objPath));
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }

        // Extract the portion after system inventory path
        // Expected format: /xyz/openbmc_project/inventory/system/chassis0/...
        const auto l_afterSystemPath = i_objPath.substr(
            l_systemInvPathPos + std::string_view(constants::systemInvPath).length());

        // Find the next '/' after "system" to locate chassis ID
        if (l_afterSystemPath.empty() || l_afterSystemPath[0] != '/')
        {
            m_logger->logMessage(std::format(
                "Invalid path format after system inventory path: {}", i_objPath));
            return std::unexpected(error_code::INVALID_INVENTORY_PATH);
        }

        // Skip the leading '/'
        const auto l_pathAfterSlash = l_afterSystemPath.substr(1);
        
        // Find the next '/' to get the chassis ID
        const auto l_nextSlashPos = l_pathAfterSlash.find('/');
        
        // Extract chassis ID (e.g., "chassis0")
        std::string_view l_chassisId;
        if (l_nextSlashPos != std::string_view::npos)
        {
            l_chassisId = l_pathAfterSlash.substr(0, l_nextSlashPos);
        }
        else
        {
            // No more slashes, the entire remaining string is the chassis ID
            l_chassisId = l_pathAfterSlash;
        }

        // Validate that we got a chassis ID
        if (l_chassisId.empty())
        {
            m_logger->logMessage(std::format(
                "Failed to extract chassis ID from object path: {}", i_objPath));
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

std::expected<bool, error_code> ConfigManager::buildMapForFru(
    const nlohmann::json::iterator& i_fruJson) const noexcept
{
    try
    {
        bool l_rc{true};
        std::lock_guard<std::mutex> l_mapLock{m_mapsMutex};

        // For each FRU, extract Chassis ID using Object path at
        // index 0, and build EEPROM to Chassis ID Map

        const auto l_eepromPath = i_fruJson.key();

        // Check if the FRU has any sub-FRUs
        if (!i_fruJson.value().is_array() || i_fruJson.value().empty())
        {
            m_logger->logMessage(std::format(
                "FRU {} has no sub-FRUs or invalid format", l_eepromPath));
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        const auto l_baseInvObjPath =
            m_systemConfigJson["frus"][l_eepromPath].at(0).value(
                "inventoryPath", "");

        if (l_baseInvObjPath.empty())
        {
            // skip this FRU
            m_logger->logMessage(std::format(
                "Base object path is empty for FRU {}", l_eepromPath));
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        // extract chassis ID string from the base inventory object path
        const auto l_chassisIdRes =
            getChassisIdFromObjectPath(l_baseInvObjPath);
        if (!l_chassisIdRes.has_value())
        {
            // skip this FRU - propagate the error
            return std::unexpected(l_chassisIdRes.error());
        }

        const auto& l_chassisId = l_chassisIdRes.value();

        // add entry to EEPROM path to chassis ID map
        m_eepromToChassisIdMap.emplace(l_eepromPath, l_chassisId);

        // iterate through the sub FRUs and append to chassis info map
        for (const auto& l_subFru : i_fruJson.value())
        {
            // Each sub-FRU should have an inventoryPath
            if (l_subFru.contains("inventoryPath"))
            {
                const auto l_subFruInvPath = l_subFru.value("inventoryPath", "");
                
                // Extract chassis ID from sub-FRU inventory path
                const auto l_subChassisIdRes =
                    getChassisIdFromObjectPath(l_subFruInvPath);
                
                if (l_subChassisIdRes.has_value())
                {
                    const auto& l_subChassisId = l_subChassisIdRes.value();
                    
                    // Add or update entry in chassis ID to chassis JSON config map
                    // If chassis ID already exists, we append to its JSON array
                    auto& l_chassisJson =
                        const_cast<std::unordered_map<std::string, nlohmann::json>&>(
                            m_chassisIdToJsonMap)[std::string(l_subChassisId)];
                    
                    // Initialize as array if it doesn't exist
                    if (l_chassisJson.is_null())
                    {
                        l_chassisJson = nlohmann::json::array();
                    }
                    
                    // Append this sub-FRU to the chassis JSON
                    l_chassisJson.push_back(l_subFru);
                }
            }
        }
        return l_rc;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Failed to build maps for FRU {}. Error: {}",
                        i_fruJson.key(), l_ex.what()));
        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}
} // namespace vpd
