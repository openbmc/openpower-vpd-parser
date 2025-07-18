#pragma once

#include "backup_restore.hpp"
#include "gpio_monitor.hpp"
#include "listener.hpp"
#include "worker.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <memory>

namespace vpd
{
/**
 * @brief Class to handle OEM specific use case.
 *
 * Few pre-requisites needs to be taken case specifically, which will be
 * encapsulated by this class.
 */
class IbmHandler
{
  public:
    /**
     * List of deleted methods.
     */
    IbmHandler(const IbmHandler&) = delete;
    IbmHandler& operator=(const IbmHandler&) = delete;
    IbmHandler(IbmHandler&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param[in] o_worker - Reference to worker class object.
     * @param[in] o_backupAndRestoreObj - Ref to back up and restore class
     * object.
     * @param[in] i_iFace - interface to implement.
     * @param[in] i_ioCon - IO context.
     * @param[in] i_asioConnection - Dbus Connection.
     */
    IbmHandler(
        std::shared_ptr<Worker>& o_worker,
        std::shared_ptr<BackupAndRestore>& o_backupAndRestoreObj,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_iFace,
        const std::shared_ptr<boost::asio::io_context>& i_ioCon,
        const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection);

    /**
     * @brief API to collect all FRUs VPD.
     *
     * This api will call worker API to perform VPD collection for all FRUs
     * present in the system config JSON and publish it on DBus. Also updates
     * the Dbus VPD collection status property hosted under vpd-manager.
     *
     * Note:
     * System VPD collection will always be skipped.
     * If host is in power on state, FRUs marked as 'powerOffOnly' in the
     * system config JSON will be skipped.
     *
     * @throw JsonException, runtime_error
     */
    void collectAllFruVpd();

  private:
    /**
     * @brief API to set timer to detect system VPD over D-Bus.
     *
     * System VPD is required before bus name for VPD-Manager is claimed. Once
     * system VPD is published, VPD for other FRUs should be collected. This API
     * detects id system VPD is already published on D-Bus and based on that
     * triggers VPD collection for rest of the FRUs.
     *
     * Note: Throws exception in case of any failure. Needs to be handled by the
     * caller.
     */
    void SetTimerToDetectSVPDOnDbus();

    /**
     * @brief Set timer to detect and set VPD collection status for the system.
     *
     * Collection of FRU VPD is triggered in a separate thread. Resulting in
     * multiple threads at  a given time. The API creates a timer which on
     * regular interval will check if all the threads were collected back and
     * sets the status of the VPD collection for the system accordingly.
     *
     * @throw std::runtime_error
     */
    void SetTimerToDetectVpdCollectionStatus();

    /**
     * @brief API to process VPD collection thread failed EEPROMs.
     */
    void processFailedEeproms();

    /**
     * @brief API to check and update PowerVS VPD.
     *
     * The API will read the existing data from the DBus and if found
     * different than what has been read from JSON, it will update the VPD with
     * JSON data on hardware and DBus both.
     *
     * @param[in] i_powerVsJsonObj - PowerVS JSON object.
     * @param[out] o_failedPathList - List of path failed to update.
     */
    void checkAndUpdatePowerVsVpd(const nlohmann::json& i_powerVsJsonObj,
                                  std::vector<std::string>& o_failedPathList);
    /**
     * @brief API to handle configuration w.r.t. PowerVS systems.
     *
     * Some FRUs VPD is specific to powerVS system. The API detects the
     * powerVS configuration and updates the VPD accordingly.
     */
    void ConfigurePowerVsSystem();

    /**
     * @brief API to perform initial setup before manager claims Bus name.
     *
     * Before BUS name for VPD-Manager is claimed, fitconfig whould be set for
     * corret device tree, inventory JSON w.r.t system should be linked and
     * system VPD should be on DBus.
     */
    void performInitialSetup();

    /**
     * @brief API to prime system blueprint.
     *
     * The API will traverse the system config JSON and will prime all the FRU
     * paths which qualifies for priming.
     */
    void primeSystemBlueprint();

    /**
     * @brief Function to enable and bring MUX out of idle state.
     *
     * This finds all the MUX defined in the system json and enables them by
     * setting the holdidle parameter to 0.
     *
     * @throw std::runtime_error
     */
    void enableMuxChips();

    /**
     * @brief API to check if priming is required.
     *
     * The API will traverse the system config JSON and counts the FRU
     * paths which qualifies for priming and compares with count of object paths
     * found under PIM which hosts the "com.ibm.VPD.Collection" interface. If
     * the dbus count is equal to or greater than the count from JSON config
     * consider as priming is not required.
     *
     * @return true if priming is required, false otherwise.
     */
    bool isPrimingRequired() const noexcept;

    // Parsed system config json object.
    nlohmann::json m_sysCfgJsonObj{};

    // Shared pointer to worker class
    std::shared_ptr<Worker>& m_worker;

    // Shared pointer to backup and restore object.
    std::shared_ptr<BackupAndRestore>& m_backupAndRestoreObj;

    // Shared pointer to GpioMonitor object.
    std::shared_ptr<GpioMonitor> m_gpioMonitor;

    // Shared pointer to Dbus interface class.
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& m_interface;

    // Shared pointer to asio context object.
    const std::shared_ptr<boost::asio::io_context>& m_ioContext;

    // Shared pointer to bus connection.
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConnection;

    // Shared pointer to Listener object.
    std::shared_ptr<Listener> m_eventListener;
};
} // namespace vpd
