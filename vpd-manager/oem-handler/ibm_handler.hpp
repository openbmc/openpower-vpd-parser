#pragma once

#include "backup_restore.hpp"
#include "gpio_monitor.hpp"
#include "listener.hpp"
#include "logger.hpp"

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
     * @param[in] o_backupAndRestoreObj - Ref to back up and restore class
     * object.
     * @param[in] i_iFace - interface to implement.
     * @param[in] i_progressiFace - Interface to track collection progress.
     * @param[in] i_ioCon - IO context.
     * @param[in] i_asioConnection - Dbus Connection.
     * @param[in] i_vpdCollectionMode - VPD collection mode.
     */
    IbmHandler(
        std::shared_ptr<BackupAndRestore>& o_backupAndRestoreObj,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_iFace,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_progressiFace,
        const std::shared_ptr<boost::asio::io_context>& i_ioCon,
        const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection,
        const types::VpdCollectionMode& i_vpdCollectionMode);

    /**
     * @brief API to register listener objects.
     *
     * @param[in] i_eventListener - shared pointer to Listener object
     */
    void initIbmListenerObject(
        std::shared_ptr<Listener>& i_eventListener) noexcept;

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
     * @param[in] i_fruPath - System VPD EEPROM path.
     * @param[out] o_parsedSystemVpdMap - Parsed system VPD map.
     */
    void setDeviceTreeAndJson(const std::string& i_fruPath,
                              types::VPDMapVariant& o_parsedSystemVpdMap);

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
     * @brief Reset data under non system inventory paths
     *
     * This method updates the object map containing system inventory to reset
     * data under all inventory paths other than system inventory path.
     *
     * @param[in,out] io_objectMap - Object map to be filtered. On success, it
     * contains the updated map with data under all inventory paths other than
     * system inventory path reset to default values.
     */
    void resetNonSystemInvPaths(types::ObjectMap& io_objectMap) const noexcept;

    /**
     * @brief API to perform initial setup before manager claims Bus name.
     *
     * Before BUS name for VPD-Manager is claimed, fitconfig would be set for
     * correct device tree, inventory JSON w.r.t system should be linked and
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
     * @brief API to initialize back up and restore class.
     */
    void initBackupAndRestore() noexcept;

    /**
     * @brief Callback API to handle collection status changes.
     *
     * This listener is registered by IBM handler to watch collection status
     * updates.
     *
     * @param[in] i_msg - Callback message.
     */
    void collectionStatusChangeCallback(
        sdbusplus::message_t& i_msg) const noexcept;

    /**
     * @brief API to update VPD collection status property
     *
     * This API updates the VPD collection status property on D-Bus and then
     * triggers a signal emission to indicate change in the VPD collection
     * status property
     *
     * @param[in] i_status - VPD collection status value
     */
    void updateVpdCollectionStatus(
        const types::VpdCollectionStatus i_status) const noexcept;

    /**
     * @brief API to add or restore the availability property for inventory
     * objects.
     *
     * This API iterates through all inventory paths in the object map and
     * checks if the availability property already exists under PIM. If not,
     * it populates the property with default value "false".
     *
     * @param[in,out] io_objectInterfaceMap - Object interface map to update.
     */
    void addOrRestoreAvailableProperty(types::ObjectMap& io_objectInterfaceMap);

    /**
     * @brief API to validate the VPD collection mode.
     *
     * This API validates the VPD collection mode. If the mode is not valid, it
     * throws an exception.
     *
     * @throw FirmwareException if the VPD collection mode is invalid.
     */
    void validateVpdCollectionMode() const;

    /**
     *  @brief API to handle BMC ReadyToRemove property
     *
     *  This API handles ReadyToRemove interface property for BMC. ReadyToRemove
     * property is used by Concurrent Maintenance flow to identify whether a FRU
     * is ready to be replaced. On redundant BMC systems, only Passive BMC is
     * concurrently maintenable and hence only Passive BMC should have the
     * ReadyToRemove property.
     *
     *  @return - On success returns 0, otherwise returns -1
     */
    int handleBmcReadyToRemove() const noexcept;

    // Parsed system config json object.
    nlohmann::json m_sysCfgJsonObj{};

    // Shared pointer to backup and restore object.
    std::shared_ptr<BackupAndRestore>& m_backupAndRestoreObj;

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
