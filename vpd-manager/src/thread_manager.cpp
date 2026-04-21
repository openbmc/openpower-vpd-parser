#include "thread_manager.hpp"

#include <format>

namespace vpd
{

ThreadManager::ThreadManager(
    const std::shared_ptr<Worker>& i_worker,
    const std::shared_ptr<ConfigManager>& i_configManager) :
    m_configManager(i_configManager), m_worker(i_worker)
{}

} // namespace vpd
