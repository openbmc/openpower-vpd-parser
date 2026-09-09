#pragma once

#include "constants.hpp"
#include "logger.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
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
        m_logger{vpd::Logger::getLoggerInstance()}
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
     * @brief Reads a property value from a PIM-serialised backup file.
     *
     * Opens the cereal JSON file at @p i_filePath, and returns the value
     * of @p i_propertyKey found under the fixed "value0" wrapper object.
     *
     * @param[in] i_filePath    - Path to the serialised property file.
     * @param[in] i_propertyKey - JSON key of the property to read.
     *
     * @return The property value as nlohmann::json, or nlohmann::json{} (null)
     *         if the file cannot be opened, parsed, or the key is absent.
     */
    nlohmann::json readPropertyFromBackupFile(
        const std::filesystem::path& i_filePath,
        const std::string& i_propertyKey) const noexcept;

    /**
     * @brief Identifies BMC inventory paths in the backup tree.
     *
     * Walks the backup PIM root, locates every serialised
     * PhysicalContext property file, and returns the set of parent directory
     * paths whose Type property equals the Manager ordinal (1).
     *
     * @return Set of absolute path strings of BMC inventory directories found
     *         in the backup tree. Empty if none are found or on error.
     */
    std::unordered_set<std::string> getBMCPathsFromBackup() const noexcept;

    /**
     * @brief Decides whether an interface directory should be skipped during
     *        backup restoration.
     *
     * Returns true when both conditions hold:
     *   1. The entry's filename is found in the compile-time skip set.
     *   2. The entry's parent path is found in the runtime BMC path set.
     *
     * @param[in] i_entryPath - Filesystem path of the candidate interface
     *                          directory entry.
     *
     * @return true if the entry should be skipped, false otherwise.
     */
    bool shouldSkipInterfaceRestore(
        const std::filesystem::path& i_entryPath) const noexcept;

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
     * @brief BMC inventory paths identified in the backup tree.
     *
     * Populated by restoreInventoryBackupData() before moveDirectory() is
     * called. Read by shouldSkipInterfaceRestore() inside the traversal loop.
     * Mutable so that const restore methods can populate it.
     */
    mutable std::unordered_set<std::string> m_bmcPaths;

    /**
     * @brief Compile-time set of interface names that must never be restored
     *        for BMC inventory paths during failover.
     *
     * Add an entry here to suppress additional interfaces in the future
     * without changing any other logic.
     */
    static const std::unordered_set<std::string> m_skipInterfaceSet;
};
