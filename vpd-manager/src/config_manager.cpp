#include "config_manager.hpp"

#include "constants.hpp"

namespace vpd
{
const nlohmann::json& ConfigManager::getJsonObj(
    const std::optional<std::string>& i_vpdPath) const noexcept
{
    if (!i_vpdPath)
    {
        return m_systemConfigJson;
    }

    const auto& l_vpdPath = i_vpdPath.value();
    std::string l_chassisId{};

    if (l_vpdPath.starts_with(constants::pimPath))
    {
        l_chassisId = getChassisId(l_vpdPath);
    }
    else if (m_eepromToChassisIdMap.find(l_vpdPath) !=
             m_eepromToChassisIdMap.end())
    {
        l_chassisId = m_eepromToChassisIdMap.at(l_vpdPath);
    }
    else
    {
        return m_systemConfigJson;
    }

    if (m_chassisIdToJsonMap.find(l_chassisId) != m_chassisIdToJsonMap.end())
    {
        return m_chassisIdToJsonMap.at(l_chassisId);
    }

    return m_systemConfigJson;
}

std::string ConfigManager::getChassisId(
    const std::string& i_inventoryObjPath) const noexcept
{
    try
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
    catch (const std::exception& l_ex)
    {
        //@todo add a log
        return std::string{};
    }
}
} // namespace vpd
