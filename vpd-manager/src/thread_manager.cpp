#include "thread_manager.hpp"

#include <format>

namespace vpd
{

ThreadManager::ThreadManager(
    const std::shared_ptr<Worker>& i_worker,
    const std::shared_ptr<ConfigManager>& i_configManager) :
    m_configManager(i_configManager), m_worker(i_worker)
{}

bool ThreadManager::collectAllChassisVpd()
{
    // Get the chassisId to json map
    const auto& l_chassisIdToJsonMap = m_configManager->getChassisIdToJsonMap();

    /*TODO:- Get motherboard's EEPROM path and from respective
    configmanager api and spawn threads which will take care of VPD
    collection of each chassis motherboards */

    return true;
}

} // namespace vpd
