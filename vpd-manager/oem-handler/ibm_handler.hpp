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
     * @param[in] backupAndRestoreObj - Ref to back up and restore class
     * object.
     * @param[in] iFace - interface to implement.
     * @param[in] progressIFace - Interface to track collection progress.
     * @param[in] ioCon - IO context.
     * @param[in] asioConnection - Dbus Connection.
     * @param[in] vpdCollectionMode - VPD collection mode.
     */
    IbmHandler(
        std::shared_ptr<BackupAndRestore>& backupAndRestoreObj,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& iFace,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& progressIFace,
        const std::shared_ptr<boost::asio::io_context>& ioCon,
        const std::shared_ptr<sdbusplus::asio::connection>& asioConnection,
        const types::VpdCollectionMode& vpdCollectionMode);

    /**
     * @brief API to register listener objects.
     *
     * @param[in] eventListener - shared pointer to Listener object
     */
    void initIbmListenerObject(
        std::shared_ptr<Listener>& eventListener) noexcept;

  private:
    /**
     * @brief API to collect system VPD and set appropriate device tree and
     * JSON.
     *
     * This API based on system chooses corresponding device tree and JSON.
     * If device tree change is required, it updates the "fitconfig" and reboots
     * the system. Else it is NOOP.
     *
     * @throw std::exception
     *
     * @param[in] fruPath - System VPD EEPROM path.
     * @param[out] parsedSystemVpdMap - Parsed system VPD map.
     */
    void setDeviceTreeAndJson(const std::string& fruPath,
                              types::VPDMapVariant& parsedSystemVpdMap);

    /**
     * @brief API to detect if system vpd is backed up in cache.
     *
     * System vpd can be cached either in cache or some other location. The
     * information is extracted from system config json.
     *
     * @return True if the location is cache, false otherwise.
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
     * @param[out] systemJson - System JSON name.
     * @param[in] parsedVpdMap - Parsed VPD map.
     */
    void getSystemJson(std::string& systemJson,
                       const types::VPDMapVariant& parsedVpdMap);

    /**
     * @brief An API to perform backup or restore of VPD.
     *
     * @param[in,out] srcVpdMap - Source VPD map.
     */
    void performBackupAndRestore(types::VPDMapVariant& srcVpdMap);

    /**
     *  @brief An API to parse and publish system VPD on D-Bus.
     *
     * @throw DataException, std::runtime_error
     *
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     */
    void publishSystemVPD(const types::VPDMapVariant& parsedVpdMap);

    /**
     * @brief API to form asset tag string for the system.
     *
     * @param[in] parsedVpdMap - Parsed VPD map.
     *
     * @throw std::runtime_error
     *
     * @return - Formed asset tag string.
     */
    std::string createAssetTagString(const types::VPDMapVariant& parsedVpdMap);

    /**
     * @brief Reset data under non system inventory paths
     *
     * This method updates the object map containing system inventory to reset
     * data under all inventory paths other than system inventory path.
     *
     * @param[in,out] objectMap - Object map to be filtered. On success, it
     * contains the updated map with data under all inventory paths other than
     * system inventory path reset to default values.
     */
    void resetNonSystemInvPaths(types::ObjectMap& objectMap) const noexcept;

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
     * Once correct device tree is set, symbolic link to the correct system
     * config JSON is set to be used in subsequent BMC boot.
     *
     * @throws std::runtime_error
     *
     * @param[in] systemJson - system config JSON.
     */
    void setJsonSymbolicLink(const std::string& systemJson);

    /**
     * @brief API to set environment variable and reboot the BMC
     *
     * @param[in] key - Name of the environment variable
     * @param[in] value - Value of the environment variable
     *
     * @throw std::runtime_error
     */
    void setEnvAndReboot(const std::string& key, const std::string& value);

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
     * @param[in] msg - Callback message.
     */
    void collectionStatusChangeCallback(
        sdbusplus::message_t& msg) const noexcept;

    /**
     * @brief API to add or restore the availability property for inventory
     * objects.
     *
     * This API iterates through all inventory paths in the object map and
     * checks if the availability property already exists under PIM. If not,
     * it populates the property with default value "false".
     *
     * @param[in,out] objectInterfaceMap - Object interface map to update.
     */
    void addOrRestoreAvailableProperty(types::ObjectMap& objectInterfaceMap);

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
     * @brief API to handle BMC ReadyToRemove property
     *
     * This API handles ReadyToRemove interface property for BMC. ReadyToRemove
     * property is used by Concurrent Maintenance flow to identify whether a FRU
     * is ready to be replaced. On redundant BMC systems, only Passive BMC is
     * concurrently maintainable and hence only Passive BMC should have the
     * ReadyToRemove property.
     *
     * @return - On success returns 0, otherwise returns -1
     */
    int handleBmcReadyToRemove() const noexcept;

    // Parsed system config json object.
    nlohmann::json sysCfgJsonObj{};

    // Shared pointer to backup and restore object.
    std::shared_ptr<BackupAndRestore>& backupAndRestoreObj;

    // Shared pointer to Dbus interface class.
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& interface;

    // Shared pointer to Dbus collection progress interface class.
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& progressInterface;

    // Shared pointer to asio context object.
    const std::shared_ptr<boost::asio::io_context>& ioContext;

    // Shared pointer to bus connection.
    const std::shared_ptr<sdbusplus::asio::connection>& asioConnection;

    // Shared pointer to Listener object.
    std::shared_ptr<Listener> eventListener;

    // Shared pointer to Logger object.
    std::shared_ptr<Logger> logger;

    // vpd collection mode
    const types::VpdCollectionMode vpdCollectionMode;

    // Holds if symlink to config JSON is present or not.
    bool symlinkPresent = false;

    // Holds path to the config JSON being used.
    std::string configJsonPath{INVENTORY_JSON_DEFAULT};

    // To distinguish the factory reset path.
    bool isFactoryResetDone = false;
};
} // namespace vpd
