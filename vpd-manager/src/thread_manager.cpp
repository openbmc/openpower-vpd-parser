#include "thread_manager.hpp"

#include <format>

namespace vpd
{

ThreadManager::ThreadManager(
    const std::shared_ptr<Worker>& i_worker,
    const std::shared_ptr<ConfigManager>& i_configManager) :
    m_configManager(i_configManager), m_worker(i_worker)
{}

void ThreadManager::collectAllChassisVpd(
    const std::map<std::string, std::string>& i_chassisToEepromMap,
    const std::map<std::string, nlohmann::json>& i_chassisIdToJsonMap)
{
    /*
     * TODO: Implement multi-threaded VPD collection for all chassis
     * motherboards
     *
     * Implementation plan:
     * spawn threads which will take care of VPD collection of each chassis
     * motherboards
     *
     */
}

void ThreadManager::callAllFruVpd()
{
    // Get the chassis to motherboard EEPROM path map from ConfigManager
    const auto& l_chassisToMotherboardEepromMap =
        m_configManager->getChassisToMotherboardEepromMap();

    // Get the chassisId to json map for chassis-specific configuration
    const auto& l_chassisIdToJsonMap = m_configManager->getChassisIdToJsonMap();

    /*
     * TODO: Implement multi-threaded VPD collection for all FRUs in the system
     *
     * This API orchestrates VPD collection for all FRUs across all chassis.
     *
     * Implementation plan:
     *
     * 1. Create thread pool for parallel VPD collection:
     * 2. Track progress and publish status to D-Bus:
     *    a. Update collection status (In Progress, Completed, Failed)
     * 3. Handle errors gracefully:
     *
     */
    collectAllChassisVpd(l_chassisToMotherboardEepromMap, l_chassisIdToJsonMap);
}

} // namespace vpd
