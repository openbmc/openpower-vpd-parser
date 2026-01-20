#pragma once

#include "backup_restore.hpp"
#include "constants.hpp"
#include "gpio_monitor.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "worker.hpp"

#include <oem-handler/ibm_handler.hpp>
#include <sdbusplus/asio/object_server.hpp>

namespace vpd
{
/**
 * @brief Class to manage VPD processing.
 *
 * The class is responsible to implement methods to manage VPD on the system.
 * It also implements methods to be exposed over D-Bus required to access/edit
 * VPD data.
 */
class Manager
{
  public:
    /**
     * List of deleted methods.
     */
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param[in] ioCon - IO context.
     * @param[in] iFace - interface to implement.
     * @param[in] progressiFace - Interface to track collection progress.
     * @param[in] connection - Dbus Connection.
     */
    Manager(
        const std::shared_ptr<boost::asio::io_context>& ioCon,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& iFace,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& progressiFace,
        const std::shared_ptr<sdbusplus::asio::connection>& asioConnection);

    /**
     * @brief Destructor.
     */
    ~Manager() = default;

    /**
     * @brief Update keyword value.
     *
     * This API is used to update keyword value on the given input path and its
     * redundant path(s) if any taken from system config JSON.
     *
     * To update IPZ type VPD, input parameter for writing should be in the form
     * of (Record, Keyword, Value). Eg: ("VINI", "SN", {0x01, 0x02, 0x03}).
     *
     * To update Keyword type VPD, input parameter for writing should be in the
     * form of (Keyword, Value). Eg: ("PE", {0x01, 0x02, 0x03}).
     *
     * @param[in] i_vpdPath - Path (inventory object path/FRU EEPROM path).
     * @param[in] i_paramsToWriteData - Input details.
     *
     * @return On success returns number of bytes written, on failure returns
     * -1.
     */
    int updateKeyword(const types::Path i_vpdPath,
                      const types::WriteVpdParams i_paramsToWriteData);

    /**
     * @brief Update keyword value on hardware.
     *
     * This API is used to update keyword value on hardware. Updates only on the
     * given input hardware path, does not look for corresponding redundant or
     * primary path against the given path. To update corresponding paths, make
     * separate call with respective path.
     *
     * To update IPZ type VPD, input parameter for writing should be in the form
     * of (Record, Keyword, Value). Eg: ("VINI", "SN", {0x01, 0x02, 0x03}).
     *
     * To update Keyword type VPD, input parameter for writing should be in the
     * form of (Keyword, Value). Eg: ("PE", {0x01, 0x02, 0x03}).
     *
     * @param[in] i_fruPath - EEPROM path of the FRU.
     * @param[in] i_paramsToWriteData - Input details.
     *
     * @return On success returns number of bytes written, on failure returns
     * -1.
     */
    int updateKeywordOnHardware(
        const types::Path i_fruPath,
        const types::WriteVpdParams i_paramsToWriteData) noexcept;

    /**
     * @brief Read keyword value.
     *
     * API can be used to read VPD keyword from the given input path.
     *
     * To read keyword of type IPZ, input parameter for reading should be in the
     * form of (Record, Keyword). Eg: ("VINI", "SN").
     *
     * To read keyword from keyword type VPD, just keyword name has to be
     * supplied in the input parameter. Eg: ("SN").
     *
     * @param[in] i_fruPath - EEPROM path.
     * @param[in] i_paramsToReadData - Input details.
     *
     * @throw
     * sdbusplus::xyz::openbmc_project::Common::Device::Error::ReadFailure.
     *
     * @return On success returns the read value in variant of array of bytes.
     * On failure throws exception.
     */
    types::DbusVariantType readKeyword(
        const types::Path i_fruPath,
        const types::ReadVpdParams i_paramsToReadData);

    /**
     * @brief Collect single FRU VPD
     * API can be used to perform VPD collection for the given FRU, only if the
     * current state of the system matches with the state at which the FRU is
     * allowed for VPD recollection.
     *
     * @param[in] i_dbusObjPath - D-bus object path
     */
    void collectSingleFruVpd(
        const sdbusplus::message::object_path& i_dbusObjPath);

    /**
     * @brief Delete single FRU VPD
     * API can be used to perform VPD deletion for the given FRU.
     *
     * @param[in] i_dbusObjPath - D-bus object path
     */
    void deleteSingleFruVpd(
        const sdbusplus::message::object_path& i_dbusObjPath);

    /**
     * @brief Get expanded location code.
     *
     * API to get expanded location code from the unexpanded location code.
     *
     * @param[in] i_unexpandedLocationCode - Unexpanded location code.
     * @param[in] i_nodeNumber - Denotes the node in case of a multi-node
     * configuration, defaulted to zero incase of single node system.
     *
     * @throw xyz.openbmc_project.Common.Error.InvalidArgument for
     * invalid argument.
     *
     * @return Location code of the FRU.
     */
    std::string getExpandedLocationCode(
        const std::string& i_unexpandedLocationCode,
        [[maybe_unused]] const uint16_t i_nodeNumber = 0);

    /**
     * @brief Get D-Bus object path of FRUs from expanded location code.
     *
     * An API to get list of FRU D-Bus object paths for a given expanded
     * location code.
     *
     * @param[in] i_expandedLocationCode - Expanded location code.
     *
     * @throw xyz.openbmc_project.Common.Error.InvalidArgument for
     * invalid argument.
     *
     * @return List of FRUs D-Bus object paths for the given location code.
     */
    types::ListOfPaths getFrusByExpandedLocationCode(
        const std::string& i_expandedLocationCode);

    /**
     * @brief Get D-Bus object path of FRUs from unexpanded location code.
     *
     * An API to get list of FRU D-Bus object paths for a given unexpanded
     * location code.
     *
     * @param[in] i_unexpandedLocationCode - Unexpanded location code.
     * @param[in] i_nodeNumber - Denotes the node in case of a multi-node
     * configuration, defaulted to zero incase of single node system.
     *
     * @throw xyz.openbmc_project.Common.Error.InvalidArgument for
     * invalid argument.
     *
     * @return List of FRUs D-Bus object paths for the given location code.
     */
    types::ListOfPaths getFrusByUnexpandedLocationCode(
        const std::string& i_unexpandedLocationCode,
        [[maybe_unused]] const uint16_t i_nodeNumber = 0);

    /**
     * @brief Get Hardware path
     * API can be used to get EEPROM path for the given inventory path.
     *
     * @param[in] i_dbusObjPath - D-bus object path
     *
     * @return Corresponding EEPROM path.
     */
    std::string getHwPath(const sdbusplus::message::object_path& i_dbusObjPath);

    /**
     * @brief  Perform VPD recollection
     * This api will trigger parser to perform VPD recollection for FRUs that
     * can be replaced at standby.
     */
    void performVpdRecollection();

    /**
     * @brief Get unexpanded location code.
     *
     * An API to get unexpanded location code and node number from expanded
     * location code.
     *
     * @param[in] i_expandedLocationCode - Expanded location code.
     *
     * @throw xyz.openbmc_project.Common.Error.InvalidArgument for
     * invalid argument.
     *
     * @return Location code in unexpanded format and its node number.
     */
    std::tuple<std::string, uint16_t> getUnexpandedLocationCode(
        const std::string& i_expandedLocationCode);

    /**
     * @brief API to collect all FRUs VPD.
     *
     * This api will call OEM handler API to perform VPD collection for all FRUs
     * present in the system config JSON.
     *
     * Note:
     * System VPD collection will always be skipped.
     * If host is in power on state, FRUs marked as 'powerOffOnly' in the
     * system config JSON will be skipped.
     *
     * @return true on successful request made, false otherwise.
     */
    bool collectAllFruVpd() const noexcept;

  private:
    /**
     * @brief An api to check validity of unexpanded location code.
     *
     * @param[in] i_locationCode - Unexpanded location code.
     *
     * @return True/False based on validity check.
     */
    bool isValidUnexpandedLocationCode(const std::string& i_locationCode);

    /**
     * @brief API to read VPD collection mode.
     *
     * Collection mode denotes if the VPD needs to be read from file or actual
     * hardware.
     */
    void readVpdCollectionMode() noexcept;

    // Shared pointer to asio context object.
    const std::shared_ptr<boost::asio::io_context>& m_ioContext;

    // Shared pointer to Dbus interface class.
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& m_interface;

    // Shared pointer to collection progress interface class.
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& m_progressInterface;

    // Shared pointer to bus connection.
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConnection;

    // Shared pointer to worker class
    std::shared_ptr<Worker> m_worker;

    // Shared pointer to GpioMonitor class
    std::shared_ptr<GpioMonitor> m_gpioMonitor;

    // Variable to hold current collection status
    std::string m_vpdCollectionStatus{constants::vpdCollectionNotStarted};

    // Shared pointer to backup and restore class
    std::shared_ptr<BackupAndRestore> m_backupAndRestoreObj;

    // Shared pointer to oem specific class.
    std::shared_ptr<IbmHandler> m_ibmHandler;

    // VPD collection mode. Default is hardware mode.
    types::VpdCollectionMode m_vpdCollectionMode =
        types::VpdCollectionMode::DEFAULT_MODE;

    // Shared pointer to logger class.
    std::shared_ptr<Logger> m_logger;
};

} // namespace vpd
