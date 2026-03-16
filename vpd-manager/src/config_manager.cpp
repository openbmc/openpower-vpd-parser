#include "config_manager.hpp"

#include "constants.hpp"

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
    auto l_startPos = i_inventoryObjPath.find("/chassis");
    if (l_startPos == std::string_view::npos)
    {
        return std::string{};
    }

    ++l_startPos;
    auto l_endPos = i_inventoryObjPath.find('/', l_startPos);
    std::string_view l_chassisId =
        i_inventoryObjPath.substr(l_startPos, l_endPos - l_startPos);

    return std::string(l_chassisId);
}
} // namespace vpd
