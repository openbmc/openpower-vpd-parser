#include "thread_manager.hpp"

#include <format>

namespace vpd
{

ThreadManager::ThreadManager(std::shared_ptr<Worker>& i_worker,
                             std::shared_ptr<ConfigManager>& i_configManager) :
    m_configManager(i_worker->getConfigManager()), m_worker(i_worker)
{}

} // namespace vpd
