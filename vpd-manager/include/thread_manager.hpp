#pragma once

#include "config_manager.hpp"
#include "worker.hpp"

#include <memory>

namespace vpd
{

/**
 * @brief class to orchestrate chassis-based VPD collection and threadpool
 * lifecycle tracking
 *
 * This class acts as a central mediator between the Manager and Worker layers,
 * It encapsulates the entire threading domain,ensuring that both the Manager
 * and Worker classes remain entirely agnostic of thread execution and
 * synchronization details.
 *
 * Its primary responsibilities include:
 * - **Concurrency Abstraction**: Starting and Managing chassis-specific thread
 * pools and task scheduling for VPD collection.
 * - **State Management**: Implementing skip logic for redundant requests,
 * completion tracking, and failure handling.
 * - **Asynchronous Communication**: Monitoring background tasks and
 * publishing VPD collection status (In Progress, Failed, Completed) to D-Bus.
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
     *
     * @param[in] i_configManager - Shared pointer to the configmanager class
     */
    ThreadManager(const std::shared_ptr<ConfigManager>& i_configManager);

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
     * @brief Trigger multi-threaded VPD collection for all FRUs
     *
     * This API spawns threads to collect VPD for all FRUs in the system
     * in parallel. It orchestrates the collection process across all chassis
     * and their respective FRUs.
     */
    void collectAllFruVpd();

  private:
    // Shared pointer to ConfigManager object
    const std::shared_ptr<ConfigManager>& m_configManager{nullptr};

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger{nullptr};

    /**
     * @brief Trigger multi-threaded VPD collection of all chassis's motherboard
     *
     * This API spawns thread for each chassis motherboard
     * to collect VPD in parallel. Each thread receives:
     * - EEPROM path of the motherboard
     * - Chassis-specific JSON configuration
     * The method uses the Worker API to perform actual VPD collection for
     * each motherboard EEPROM.
     *
     */
    void collectAllChassisVpd();
};

} // namespace vpd
