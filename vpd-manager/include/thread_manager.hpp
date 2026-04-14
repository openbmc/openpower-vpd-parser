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
 * @brief
 *
 * This class acts as a central mediator between the Manager and Worker layers,
 * It encapsulates the entire threading domain,ensuring that both the Manager
 * and Worker classes remain entirely agnostic of thread execution and
 * synchronization details.
 *
 * Its primary responsibilities include:
 * - **Concurrency Abstraction**: Starting and Managing chassis-specific thread
 * pools and task scheduling.
 * - **State Management**: Implementing skip logic for redundant requests,
 * completion tracking, and failure handling.
 * - **Asynchronous Communication**: Monitoring background tasks and
 * publishing status (In Progress, Failed, Completed) to D-Bus.
 *
 * By strictly isolating the threading logic here, the architecture
 * achieves loose coupling, allowing the classes on data processing
 * without awareness of the underlying execution model.
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

  private:
    // Shared pointer to ConfigManager object
    const std::shared_ptr<ConfigManager>& m_configManager{nullptr};

    // shared pointer to worker object
    const std::shared_ptr<Worker>& m_worker{nullptr};
};

} // namespace vpd
