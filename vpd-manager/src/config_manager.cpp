#include "config_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "utility/common_utility.hpp"
#include "utility/json_utility.hpp"

#include <format>
namespace vpd
{
void ConfigManager::buildChassisToFruMap()
{
    /* TODO:
      1. read chassis specific JSON tag from system config JSON
      2. get list of chassis specific JSONs from /usr/share/vpd
            2.i. Parse all chassis specific JSONs and load them onto
      m_chassisIdToJsonMap map
      3. get EEPROM to chassis map from /usr/share/vpd
            3.1. Parse EEPROM to chassis map and load it on
      m_eepromToChassisIdMap

    */

    if (!m_systemConfigJson.contains("chassisSpecificJsonPaths") ||
        !m_systemConfigJson.contains("eepromToChassisJsonPath"))
    {
        // not a multi-chassis sytem
        throw JsonException{"No chassis specific JSONs found"};
    }

    uint16_t l_errCode{constants::VALUE_0};
    // get the chassis specific JSON paths, and parse them
    const nlohmann::json& l_chassisSpecificJsonPaths =
        m_systemConfigJson["chassisSpecificJsonPaths"]
            .get_ref<const nlohmann::json::object_t&>();

    for (const auto& [l_chassisId, l_chassisSpecificJsonPath] :
         l_chassisSpecificJsonPaths.items())
    {
        const auto l_chassisSpecificJson =
            jsonUtility::getParsedJson(l_chassisSpecificJsonPath, l_errCode);
        if (l_errCode)
        {
            // TODO: decide if this is a critical failure, if yes throw
            m_logger->logMessage(std::format(
                "Failed to parse chassis specific JSON for {} from path {}. Error: {}",
                l_chassisId, l_chassisSpecificJsonPath,
                commonUtility::getErrCodeMsg(l_errCode)));
        }
        else
        {
            // push to map
            m_chassisIdToJsonMap.emplace(l_chassisId, l_chassisSpecificJson);
        }
    }

    // parse the EEPROM path to chassis ID JSON
    m_eepromToChassisIdJson = jsonUtility::getParsedJson(
        m_systemConfigJson["eepromToChassisJsonPath"], l_errCode);

    // TODO: decide if this is a critical failure, if yes throw
    if (l_errCode)
    {
        m_logger->logMessage(std::format(
            "Failed to parse EEPROM to chasis ID JSON from path {}. Error: {}",
            m_systemConfigJson["eepromToChassisJsonPath"],
            commonUtility::getErrCodeMsg(l_errCode)));
    }
}
} // namespace vpd
