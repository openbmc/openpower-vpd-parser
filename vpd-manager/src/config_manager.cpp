#include "config_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "utility/common_utility.hpp"
#include "utility/json_utility.hpp"

#include <format>

namespace vpd
{
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
        for (const auto& l_subFruJson : l_subFruJsonArray)
        {
            // get the inventory path
            if (l_subFruJson.contains("inventoryPath"))
            {
                const auto& l_inventoryPath = l_subFruJson["inventoryPath"];

                // get the unexpanded location code
                const auto l_locationCodeRes =
                    jsonUtility::getUnexpandedLocationCodeForFru(l_subFruJson);
                if (l_locationCodeRes.has_value())
                {
                    // first search to see if we have an entry for this location
                    // code
                    if (auto l_it = m_unexpandedLocCodeToInvPathMap.find(
                            l_locationCodeRes.value());
                        l_it != m_unexpandedLocCodeToInvPathMap.end())
                    {
                        // entry already exists, add this inventory path to the
                        // vector
                        l_it->second.emplace_back(l_inventoryPath);
                    }
                    else
                    {
                        // create a new entry in the map
                        m_unexpandedLocCodeToInvPathMap.emplace(std::make_pair(
                            l_locationCodeRes.value(),
                            types::ListOfPaths{l_inventoryPath}));
                    }
                }
            }
        }

        return true;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Failed to build maps for FRU {}. Error: {}",
                        i_fruJsonObj.key(), l_ex.what()));
        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

std::expected<types::ListOfPaths, error_code> ConfigManager::getInventoryPaths(
    const std::string& i_unexpandedLocationCode) const noexcept
{
    try
    {
        if (auto l_it =
                m_unexpandedLocCodeToInvPathMap.find(i_unexpandedLocationCode);
            l_it != m_unexpandedLocCodeToInvPathMap.end())
        {
            return l_it->second;
        }
        return std::unexpected(error_code::FRU_PATH_NOT_FOUND);
    }
    catch (const std::exception& l_ex)
    {
        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}
} // namespace vpd
