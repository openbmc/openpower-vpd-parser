#include "thread_manager.hpp"

#include <format>

namespace vpd
{

ThreadManager::ThreadManager(
    const std::shared_ptr<Worker>& i_worker,
    const std::shared_ptr<ConfigManager>& i_configManager) :
    m_configManager(i_configManager), m_worker(i_worker)
{}

/*TODO: This api will replace worker's `collectFrusFromJson` api*/
bool ThreadManager::collectAllMotherboardVpd()
{
    // Get the chassisId to json map
    const auto& l_chassisIdToJsonMap = m_configManager->getChassisIdToJsonMap();

    // TODO: Detailed implementation

    return true;
}

} // namespace vpd
