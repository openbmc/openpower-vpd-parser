#pragma once

#include "config_manager.hpp"
#include "worker.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <string>

namespace vpd
{

/**
 * @brief Class to manage threads and access configuration
 *
 * This class holds a ConfigManager object and provides access to its public
 * APIs.
 */
class ThreadManager
{
  public:
    /**
     * @brief ThreadManager Constructor
     */
    ThreadManager(std::shared_ptr<Worker>& i_worker);

    // deleted methods
    ThreadManager(const ThreadManager&) = delete;
    ThreadManager& operator=(const ThreadManager&) = delete;
    ThreadManager(ThreadManager&&) = delete;
    ThreadManager& operator=(ThreadManager&&) = delete;

    /**
     * @brief Destructor
     */
    ~ThreadManager() = default;

    /**
     * @brief API to check if ConfigManager is initialized
     *
     * @return true if ConfigManager is initialized, false otherwise
     */
    bool isConfigManagerInitialized() const noexcept;

    /**
     *  @brief API to get the json from given path
     */
    const nlohmann::json& getConfigJson(
        const std::optional<std::string>& i_vpdPath) const noexcept;

  private:
    // Unique pointer to ConfigManager object
    const std::unique_ptr<ConfigManager>& m_configManager{nullptr};
};

} // namespace vpd
