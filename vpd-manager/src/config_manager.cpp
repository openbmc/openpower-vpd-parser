#include "config_manager.hpp"

namespace vpd
{
const nlohmann::json& ConfigManager::getJsonObj(
    [[maybe_unused]] const std::optional<std::string>& i_vpdPath) const noexcept
{
    /**
     * @todo Implement the following logic:
     *  - If @p i_vpdPath is an EEPROM path, obtain the chassisId from
     * m_eepromToChassisIdMap.
     * - If @p i_vpdPath is an inventory path, extract the chassisId from the
     * path.
     *  - Return the chassis-specific JSON configuration from m_chassisInfoMap
     * using the resolved chassisId.
     *  - If @p i_vpdPath is std::nullopt, return m_systemConfigJson.
     */

    return m_systemConfigJson;
}
} // namespace vpd
