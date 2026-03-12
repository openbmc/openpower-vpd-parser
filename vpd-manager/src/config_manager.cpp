#include "config_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"

namespace vpd
{
void ConfigManager::buildChassisToFruMap()
{
    /* TODO:
        1.i. For each FRU, iterate through the sub FRUS
              1.i.i. . 1.i.ii. For each sub FRU,
      use the object path to get the chassis ID, and add the sub JSON to the
      Chassis ID to Chassis Info Map.

    */

    if (!m_systemConfigJson.contains("frus"))
    {
        throw JsonException{"System config JSON is invalid"};
    }

    // Iterate through "frus" under system config JSON
    const nlohmann::json& l_listOfFrus =
        m_systemConfigJson["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& l_aFru : l_listOfFrus.items())
    {
        // For each FRU, extract Chassis ID using Object path at
        // index 0, and build EEPROM to Chassis ID Map

        const auto l_eepromPath = l_aFru.key();

        const auto l_baseInvObjPath =
            m_systemConfigJson["frus"][l_eepromPath].at(0).value(
                "inventoryPath", "");

        if (l_baseInvObjPath.empty())
        {
            // skip this FRU
            m_logger->logMessage(std::format(
                "Base object path is empty for FRU {}", l_eepromPath));
            continue;
        }

        // extract chassis ID string from the base inventory object path
        const auto l_chassisIdRes =
            getChassisIdFromObjectPath(l_baseInvObjPath);
        if (!l_chassisIdRes.has_value())
        {
            // skip this FRU
            continue;
        }

        const auto& l_chassisId = l_chassisIdRes.value();

        // add entry to EEPROM path to chassis ID map
        m_eepromToChassisIdMap.emplace(l_eepromPath, l_chassisId);

        // iterate through the sub FRUs and append to chassis info map
        for (const auto& l_subFru : l_aFru.value())
        {
            (void)l_subFru;
            // add entry to chassis ID to chassis JSON config map
            // m_chassisInfoMap.emplace(std::make_pair<std::string,nlohmann::json>(l_chassisId,));
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

        // const auto l_systemInvPathPos =
        // i_objPath.find(constants::systemInvPath);
        return constants::systemInvPath;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to extract chassis ID from object path {}. Error: {}",
            i_objPath, l_ex.what()));
    }
    return std::unexpected(error_code::STANDARD_EXCEPTION);
}
} // namespace vpd
