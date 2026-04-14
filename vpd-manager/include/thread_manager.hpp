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
 * @brief Thread-safe wrapper for accessing ConfigManager and Worker class
 * APIs.
 *
 * This class provides thread-safe access to the ConfigManager and worker class.
 * It holds a const reference to ConfigManager and worker and will be used by
 * multiple threads to query configuration data during VPD collection and
 * processing.
 */
class ThreadManager
{
  public:
    /**
     * @brief ThreadManager Constructor
     */
    ThreadManager(std::shared_ptr<Worker>& i_worker,
                  std::shared_ptr<Worker>& i_configManager);

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
     *  @brief API to get the json from given path
     *
     *  @param[in] i_vpdPath - Optional EEPROM or inventory object path.
     */
    const nlohmann::json& getConfigJson(
        const std::optional<std::string>& i_vpdPath) const noexcept;

  private:
    // Shared pointer to ConfigManager object
    const std::shared_ptr<ConfigManager>& m_configManager{nullptr};

    // shared pointer to worker object
    const std::shared_ptr<Worker>& m_worker{nullptr};
};

} // namespace vpd
