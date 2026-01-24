#pragma once

#include "backup_restore.hpp"
#include "gpio_monitor.hpp"
#include "listener.hpp"
#include "logger.hpp"
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
     * @param[in] i_progressiFace - Interface to track collection progress.
     * @param[in] i_ioCon - IO context.
     * @param[in] i_asioConnection - Dbus Connection.
     * @param[in] i_vpdCollectionMode - VPD collection mode.
     */
    IbmHandler(
        std::shared_ptr<Worker>& o_worker,
        std::shared_ptr<BackupAndRestore>& o_backupAndRestoreObj,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_iFace,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_progressiFace,
        const std::shared_ptr<boost::asio::io_context>& i_ioCon,
        const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection,
        const types::VpdCollectionMode& i_vpdCollectionMode);

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
     * @brief API tocollect system VPD and set appropriate device tree and JSON.
     *
     * This API based on system chooses corresponding device tree and JSON.
     * If device tree change is required, it updates the "fitconfig" and reboots
     * the system. Else it is NOOP.
     *
     * @throw std::exception
     *
     * @param[out] o_parsedSystemVpdMap - Parsed system VPD map.
     */
    void setDeviceTreeAndJson(types::VPDMapVariant& o_parsedSystemVpdMap);

    /**
     * @brief API to detect if system vpd is backed up in cache.
     *
     * System vpd can be cached either in cache or some other location. The
     * information is extracted from system config json.
     *
     * @return True if the location is cache, fale otherwise.
     */
    bool isBackupOnCache();

    /**
     * @brief API to select system specific JSON.
     *
     * The API based on the IM value of VPD, will select appropriate JSON for
     * the system. In case no system is found corresponding to the extracted IM
     * value, error will be logged.
     *
     * @throw DataException, std::exception
     *
     * @param[out] o_systemJson - System JSON name.
     * @param[in] i_parsedVpdMap - Parsed VPD map.
     */
    void getSystemJson(std::string& o_systemJson,
                       const types::VPDMapVariant& i_parsedVpdMap);

    /**
     * @brief An API to perform backup or restore of VPD.
     *
     * @param[in,out] io_srcVpdMap - Source VPD map.
     */
    void performBackupAndRestore(types::VPDMapVariant& io_srcVpdMap);

    /**
     *  @brief An API to parse and publish system VPD on D-Bus.
     *
     * @throw DataException, std::runtime_error
     *
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     */
    void publishSystemVPD(const types::VPDMapVariant& i_parsedVpdMap);

    /**
     * @brief API to form asset tag string for the system.
     *
     * @param[in] i_parsedVpdMap - Parsed VPD map.
     *
     * @throw std::runtime_error
     *
     * @return - Formed asset tag string.
     */
    std::string createAssetTagString(
        const types::VPDMapVariant& i_parsedVpdMap);

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
     * @brief Function to enable and bring MUX out of idle state.
     *
     * This finds all the MUX defined in the system json and enables them by
     * setting the holdidle parameter to 0.
     *
     * @throw std::runtime_error
     */
    void enableMuxChips();

    /**
     * @brief Checks whether the system is an RBMC prototype.
     *
     * @param[out] o_errCode - To set error code in case of error.
     *
     * @return true for RBMC prototype system, false otherwise.
     */
    bool isRbmcPrototypeSystem(uint16_t& o_errCode) const noexcept;

    /**
     * @brief Checks and updates BMC position.
     *
     * This API updates BMC position for the RBMC prototype
     * system based on whether the motherboard EEPROM is accessible.
     *
     * @param[out] o_bmcPosition - BMC position.
     */
    void checkAndUpdateBmcPosition(size_t& o_bmcPosition) const noexcept;

    /**
     * @brief API to check sysconfig json symlink.
     */
    void isSymlinkPresent() noexcept;

    /** @brief API to set symbolic link for system config JSON.
     *
     * Once correct device tree is set, symbolic link to the correct sytsem
     * config JSON is set to be used in subsequent BMC boot.
     *
     * @throws std::runtime_error
     *
     * @param[in] i_systemJson - system config JSON.
     */
    void setJsonSymbolicLink(const std::string& i_systemJson);

    /**
     * @brief API to initialize worker object.
     *
     * @throws std::runtime_error.
     */
    void initWorker();

    /**
     * @brief API to set environment variable and reboot the BMC
     *
     * @param[in] i_key - Name of the environment variable
     * @param[in] i_value - Value of the environment variable
     *
     * @throw std::runtime_error
     */
    void setEnvAndReboot(const std::string& i_key, const std::string& i_value);

    /**
     * @brief API to read the fitconfig environment variable
     *
     * @return On success, returns the value of the fitconfig environment
     * variable, otherwise returns empty string
     */
    std::string readFitConfigValue();

    /**
     * @brief API to set BMC position.
     *
     * The API sets BMC position under system inventory path. In case of any
     * error, default value will be set as BMC position.
     */
    void setBmcPosition();

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

    // Shared pointer to Dbus collection progress interface class.
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& m_progressInterface;

    // Shared pointer to asio context object.
    const std::shared_ptr<boost::asio::io_context>& m_ioContext;

    // Shared pointer to bus connection.
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConnection;

    // Shared pointer to Listener object.
    std::shared_ptr<Listener> m_eventListener;

    // Shared pointer to Logger object.
    std::shared_ptr<Logger> m_logger;

    // vpd collection mode
    const types::VpdCollectionMode m_vpdCollectionMode;

    // Holds if sysmlink to config JSON is present or not.
    bool m_isSymlinkPresent = false;

    // Holds path to the config JSON being used.
    std::string m_configJsonPath{INVENTORY_JSON_DEFAULT};

    // To distinguish the factory reset path.
    bool m_isFactoryResetDone = false;
};
} // namespace vpd
