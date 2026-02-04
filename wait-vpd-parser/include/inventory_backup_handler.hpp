#pragma once

#include "constants.hpp"
#include "logger.hpp"
#include "utility/common_utility.hpp"

#include <filesystem>
#include <mutex>

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

    // Structure to hold result of path restoration
    struct RestoreResult
    {
        // file path
        std::filesystem::path path;

        // flag which indicates restoration success
        bool success;

        // error code encountered during restoration
        uint16_t errCode;

        // any additional error message (for eg. exception reason)
        std::string errMessage;

        // API to convert result to string
        std::string toString() const
        {
            return std::format(
                "Path: {}, Error code: {}, Error message: {}", path.string(),
                vpd::commonUtility::getErrCodeMsg(errCode), errMessage);
        }
    };

    /**
     * @brief API to restore inventory data of a single chassis
     *
     * This API restores inventory data of a single chassis by moving the data
     * from inventory backup path to primary path
     */
    RestoreResult restoreSingleChassis(
        const std::filesystem::path& l_src,
        const std::filesystem::path& l_dest) const noexcept;

    /* Members */
    // inventory manager service name
    std::string m_inventoryManagerServiceName;

    // inventory data primary path
    std::filesystem::path m_inventoryPrimaryPath;

    // inventory data backup path
    std::filesystem::path m_inventoryBackupPath;

    // logger instance
    std::shared_ptr<vpd::Logger> m_logger{nullptr};

    // mutex for logger instance (mutable for const methods)
    mutable std::mutex m_loggerMutex;
};
