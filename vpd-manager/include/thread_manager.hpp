#pragma once

#include "config_manager.hpp"
#include "types.hpp"
#include "worker.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

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
     * @param[in] i_progressInterface - Shared pointer to the D-Bus progress
     * interface for updating VPD collection status
     */
    ThreadManager(const std::shared_ptr<ConfigManager>& i_configManager,
                  const std::shared_ptr<sdbusplus::asio::dbus_interface>&
                      i_progressInterface);

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

    // Shared pointer to progress interface for D-Bus status updates
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& m_progressInterface{
        nullptr};

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger{nullptr};

    // Map of ChassisID to {Inventory path, Chassis presence}
    types::ChassisStateMap m_chassisStateMap;

    // Mutex to guard critical resource
    std::mutex m_mutex;

    // Tracks chassis VPD collection results awaiting processing
    std::queue<types::ChassisCollectionResult> m_chassisResultQueue;

    // Chassis VPD collection result, whose action based on the result is
    // pending
    std::atomic<size_t> m_chassisCount{0};

    // Number of FRUs pending VPD collection
    std::atomic<size_t> m_frusCount{0};

    // Condition variable to signal chassis and FRU VPD completion
    std::condition_variable m_completionCv;

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

    /**
     * @brief Updates the system view with the given chassis information.
     *
     * Retrieves the inventory object path from the chassis JSON and stores
     * the chassis state information in the system view map.
     *
     * The stored information includes:
     * - Inventory object path
     * - Presence status of the chassis
     *
     * Thread-safe access to the chassis state map is ensured using a mutex.
     *
     * @param[in] i_chassisId - Unique identifier of the chassis.
     * @param[in] i_eepromPath - EEPROM path associated with the chassis.
     * @param[in] i_chassisJson - Chassis based JSON object.
     * @param[in] i_isPresent - Indicates whether the chassis is present.
     */
    void updateSystemView(const std::string& i_chassisId,
                          const std::string& i_eepromPath,
                          const nlohmann::json& i_chassisJson,
                          const bool i_isPresent) noexcept;

    /**
     * @brief Updates overall VPD collection status on D-Bus progress interface
     *
     * This API updates the Status property on the D-Bus progress interface
     * and signals the property change. It encapsulates the D-Bus property
     * update logic for overall VPD collection status.
     *
     * @param[in] i_status - VPD collection status enum value
     */
    void updateOverallCollectionStatus(
        const types::VpdCollectionStatus i_status) const noexcept;

    /**
     * @brief Process collected chassis VPD results asynchronously.
     *
     * This API processes motherboard/chassis VPD collection results pushed into
     * the chassis task queue. For every collected chassis:
     *
     * 1. Pop chassis result from the queue.
     * 2. Verify chassis presence state.
     * 3. Launch FRU VPD collection thread pool for the chassis.
     * 4. Update FRU and chassis collection counters.
     *
     * Processing continues until all chassis's VPD collection is completed. The
     * API additionally waits until FRU VPD collection for all present chassis
     * is completed.
     */
    void processChassisResults() noexcept;

    /**
     * @brief Launch FRU VPD collection threads for a chassis.
     *
     * Creates a thread pool for the provided chassis and initiates
     * parallel VPD collection for all FRUs belonging to that chassis.
     *
     * @param[in] i_chassisEeepromPath - EEPROM path of the chassis where its
     * VPD is present.
     * @param[in] i_chassisJson - Chassis based JSON object.
     * @param[in] i_maxThreadsPerChassis - Maximum thread while collecting FRUs
     * VPD per chassis basis.
     */
    void launchFruCollectionPool(const std::string& i_chassisEeepromPath,
                                 const nlohmann::json& i_chassisJson,
                                 const size_t i_maxThreadsPerChassis) noexcept;
};

} // namespace vpd
