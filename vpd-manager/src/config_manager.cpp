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

    const auto& l_vpdPath = *i_vpdPath;
    std::string l_chassisId{};

    constexpr std::string_view l_pimPath{std::string_view(constants::pimPath)};
    if (l_vpdPath.starts_with(l_pimPath))
    {
        l_chassisId = getChassisId(l_vpdPath);
    }
    else if (const auto l_itr = m_eepromToChassisIdMap.find(l_vpdPath);
             l_itr != m_eepromToChassisIdMap.end())
    {
        l_chassisId = l_itr->second;
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
} // namespace vpd
