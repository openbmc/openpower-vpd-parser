#include "thread_manager.hpp"
#include <format>

namespace vpd
{

ThreadManager::ThreadManager(std::shared_ptr<Worker>& i_worker) :
    m_configManager(i_worker->getConfigManager())
{
}

const nlohmann::json& ThreadManager::getConfigJson(
    const std::optional<std::string>& i_vpdPath) const noexcept
{
    if (!isConfigManagerInitialized())
    {
        static const nlohmann::json l_emptyJson = nlohmann::json::object();
        return l_emptyJson;
    }

    return m_configManager->getJsonObj(i_vpdPath);
}

bool ThreadManager::isConfigManagerInitialized() const noexcept
{
    return (m_configManager != nullptr);
}

} // namespace vpd
