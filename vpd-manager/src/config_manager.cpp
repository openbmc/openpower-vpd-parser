#include "config_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "utility/common_utility.hpp"

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
                commonUtility::getErrCodeMsg(l_mapBuildResult.error())));
        }
    }

    // debug
    std::ofstream l_fileStream{"/tmp/eepromToChassisId.json"};
    for (const auto& [l_eepromPath, l_chassisId] : m_eepromToChassisIdMap)
    {
        l_fileStream << std::format("EEPROM: {} ChassisId: {}", l_eepromPath,
                                    l_chassisId)
                     << std::endl;
    }
    l_fileStream.close();

    for (const auto& [l_chassisId, l_chassisJson] : m_chassisIdToJsonMap)
    {
        std::string l_fileName{"/tmp/" + l_chassisId + ".json"};
        std::ofstream l_fileStream{l_fileName};
        l_fileStream << l_chassisJson.dump(4) << std::endl;
        l_fileStream.close();
    }
}

std::expected<bool, error_code> ConfigManager::buildMapsForFru(
    const std::string& i_eepromPath, const nlohmann::json& i_fruJson) noexcept
{
    try
    {
        bool l_rc{true};

        // Validate FRU JSON is an array with at least one element
        if (!i_fruJson.is_array() || i_fruJson.empty())
        {
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        // Extract chassis ID from inventory path of base FRU at index 0
        const auto l_baseInvObjPath =
            i_fruJson.at(0).value("inventoryPath", "");
        if (l_baseInvObjPath.empty())
        {
            return std::unexpected(error_code::INVALID_JSON);
        }

        const auto& l_chassisId = "chassis0"; // TODO

        // Create entry in EEPROM to chassis ID map
        m_eepromToChassisIdMap.emplace(i_eepromPath, std::string(l_chassisId));

        // Get or create chassis JSON object
        auto& l_chassisJson = m_chassisIdToJsonMap[std::string(l_chassisId)];
        if (l_chassisJson.is_null())
        {
            l_chassisJson = nlohmann::json::object_t();
        }

        // Check if commonInterfaces exists in system config JSON and add to the
        // entry if yes
        if (!m_systemConfigJson.contains("commonInterfaces"))
        {
            m_logger->logMessage(
                "commonInterfaces not found in system config JSON");
        }
        else if (!l_chassisJson.contains("commonInterfaces"))
        {
            // if the chassis JSON doesn't have "commonInterfaces", append it
            l_chassisJson["commonInterfaces"] =
                m_systemConfigJson["commonInterfaces"];
        }

        // check if chassis JSON has a "frus" section, if not create
        if (!l_chassisJson.contains("frus"))
        {
            l_chassisJson["frus"] = nlohmann::json::object();
        }

        // create empty array of sub FRUs
        l_chassisJson["frus"][i_eepromPath] = nlohmann::json::array();

        for (const auto& l_subFruJson : i_fruJson)
        {
            l_chassisJson["frus"][i_eepromPath] += l_subFruJson;
        }

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
