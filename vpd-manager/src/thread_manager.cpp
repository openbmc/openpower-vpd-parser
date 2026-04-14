#include "thread_manager.hpp"

#include <format>

namespace vpd
{

ThreadManager::ThreadManager(std::shared_ptr<Worker>& i_worker,
                             std::shared_ptr<ConfigManager>& i_configManager) :
    m_configManager(i_worker->getConfigManager()),
    m_worker(i_worker)
{}

const nlohmann::json& ThreadManager::getConfigJson(
    const std::optional<std::string>& i_vpdPath) const noexcept
{
    if (!m_configManager)
    {
        static const nlohmann::json l_emptyJson = nlohmann::json::object();
        return l_emptyJson;
    }

    return m_configManager->getJsonObj(i_vpdPath);
}

} // namespace vpd
