#pragma once

#include "constants.hpp"
#include "logger.hpp"
#include "utility/json_utility.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

/**
 * @brief Class to handle backup inventory data.
 *
 * This class is used for handling inventory backup data. This contains methods
 * to handle checking inventory backed up data, restoring said data and
 * restarting inventory manager service if needed
 */
class InventoryBackupHandler
{
  public:
    /*
     * Deleted methods
     */
    InventoryBackupHandler() = delete;
    InventoryBackupHandler(const InventoryBackupHandler&) = delete;
    InventoryBackupHandler operator=(const InventoryBackupHandler&) = delete;
    InventoryBackupHandler(const InventoryBackupHandler&&) = delete;
    InventoryBackupHandler operator=(const InventoryBackupHandler&&) = delete;

    /**
     * @brief Parameterized Constructor
     *
     * @param[in] i_inventoryServiceName - Inventory manager service name
     * @param[in] i_inventoryPrimaryPath - Absolute file path to inventory
     * manager primary data location
     * @param[in] i_inventoryBackupPath - Absolute file path to inventory
     * manager backup data location
     */
    InventoryBackupHandler(const std::string_view i_inventoryServiceName,
                           const std::filesystem::path i_inventoryPrimaryPath,
                           const std::filesystem::path i_inventoryBackupPath) :
        m_inventoryManagerServiceName{i_inventoryServiceName},
        m_inventoryPrimaryPath{i_inventoryPrimaryPath},
        m_inventoryBackupPath{i_inventoryBackupPath},
        m_logger{vpd::Logger::getLoggerInstance()},
        m_skipInterfaceMap{buildSkipInterfaceMap()}
    {}

    /**
     * @brief API to restore inventory data from backup file path to inventory
     * persisted path
     *
     * @param[out] o_errCode - To set error code in case of error.
     *
     * @return true if the restoration is successful, false otherwise
     *
     */
    bool restoreInventoryBackupData(uint16_t& o_errCode) const noexcept;

    /**
     * @brief API to clear inventory backup data from backup file path
     *
     * @param[out] o_errCode - To set error code in case of error.
     *
     * @return true if backup data has been cleared, false otherwise
     *
     */
    bool clearInventoryBackupData(uint16_t& o_errCode) const noexcept;

    /**
     * @brief API to restart inventory manager service
     *
     * @param[out] o_errCode - To set error code in case of error.
     *
     * @return true if inventory manager service is successfully restarted,
     * false otherwise
     *
     */
    bool restartInventoryManagerService(uint16_t& o_errCode) const noexcept;

  private:
    /**
     * @brief Build the interface skip map from BMC inventory paths.
     *
     * Fetches BMC0 and BMC1 inventory paths via
     * jsonUtility::getBmcInventoryPaths() and constructs the map that
     * shouldSkipInterfaces() consults. Non-empty paths are inserted as
     * relative filesystem path strings (no leading '/').
     *
     * @return Populated skip interface map.
     */
    static std::unordered_map<std::string, std::unordered_set<std::string>>
        buildSkipInterfaceMap() noexcept;

    /**
     * @brief API to check if inventory backup path has data
     *
     * @param[out] o_errCode - To set error code in case of error.
     *
     * @return true if inventory backup data is found, false otherwise
     *
     */
    bool checkInventoryBackupPath(uint16_t& o_errCode) const noexcept;

    /**
     * @brief API to move files from source path to destination path
     *
     * @param[in] i_src - Source path
     * @param[in] i_dest - Destination path
     *
     * @return true if the files are successfully moved, false otherwise
     */
    bool moveFiles(const std::filesystem::path& l_src,
                   const std::filesystem::path& l_dest) const noexcept;

    /**
     * @brief API to move directory from source to destination path
     *
     * This API recursively traverses the given directory path and moves
     * subdirectories that are not in the skip list.
     *
     * @param[in] i_srcPath - Source path
     * @param[in] i_dstPath - Destination path
     * @param[out] o_failedPaths - vector to hold list of failed paths
     *
     * @throw std::filesystem::filesystem_error, std::bad_alloc exceptions.
     */
    void moveDirectory(const std::filesystem::path& i_srcPath,
                       const std::filesystem::path& i_dstPath,
                       std::vector<std::filesystem::path>& o_failedPaths) const;

    /**
     * @brief API to check if an interface should be skipped for a given
     * inventory path during restoration.
     *
     * Consults the static @ref m_skipInterfaceMap. The inventory path key is
     * derived from the full relative path of the entry's parent directory, and
     * the interface name is the entry's filename. A given inventory path may
     * have multiple interfaces listed in the map, all of which will be skipped.
     *
     * @param[in] i_entryPath - Absolute filesystem path of the interface
     *                          directory entry being considered for
     *                          restoration.
     *
     * @return true if the interface should be skipped, false otherwise.
     */
    bool shouldSkipInterfaces(
        const std::filesystem::path& i_entryPath) const noexcept;

    /* Members */
    // inventory manager service name
    std::string m_inventoryManagerServiceName;

    // inventory data primary path
    std::filesystem::path m_inventoryPrimaryPath;

    // inventory data backup path
    std::filesystem::path m_inventoryBackupPath;

    // logger instance
    std::shared_ptr<vpd::Logger> m_logger{nullptr};

    /**
     * @brief Map of inventory path suffixes to sets of interface names that
     * must not be restored from backup.
     *
     * Built at construction time from the BMC inventory paths returned by
     * jsonUtility::getBmcInventoryPaths().
     *
     * Key   - Relative filesystem path of the inventory object (no leading
     *         '/'), e.g.
     *         "xyz/openbmc_project/inventory/system/chassis1/motherboard/ebmc_card"
     * Value - Set of interface names (leaf directory names) to skip at that
     *         inventory path, e.g.
     *         { "xyz.openbmc_project.State.ReadyToRemove" }
     */
    const std::unordered_map<std::string, std::unordered_set<std::string>>
        m_skipInterfaceMap;
};
