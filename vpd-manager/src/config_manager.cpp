#include "config_manager.hpp"
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
}
} // namespace vpd
