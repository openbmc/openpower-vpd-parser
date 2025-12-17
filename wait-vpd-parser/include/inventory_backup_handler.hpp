#pragma once

#include "constants.hpp"
#include "logger.hpp"

#include <filesystem>

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
    InventoryBackupHandler(const InventoryBackupHandler&) = delete;
    InventoryBackupHandler operator=(const InventoryBackupHandler&) = delete;
    InventoryBackupHandler(const InventoryBackupHandler&&) = delete;
    InventoryBackupHandler operator=(const InventoryBackupHandler&&) = delete;

    /**
     * @brief Parameterized Constructor
     *
     * @param[in] i_inventoryServiceName - Inventory manager service name
     * @param[in] i_inventoryBackupPath - Inventory backup path
     */
    InventoryBackupHandler(const std::string_view i_inventoryServiceName,
                           const std::filesystem::path i_inventoryBackupPath) :
        m_inventoryManagerServiceName{i_inventoryServiceName},
        m_inventoryBackupPath{i_inventoryBackupPath},
        m_logger{vpd::Logger::getLoggerInstance()}
    {}

    /**
     * @brief Default constructor
     *
     */
    InventoryBackupHandler() :
        InventoryBackupHandler{vpd::constants::pimServiceName,
                               vpd::constants::pimBackupPath}
    {}

    /**
     * @brief API to check and restore inventory backup data
     *
     * This API checks if there is any backed up inventory data. If backup data
     * is found at the specified file path, the back up data is restored, the
     * inventory manager service is restarted and the backup data is deleted.
     *
     * @return true if inventory backup data is found and successfully restored,
     * false otherwise
     */
    bool checkAndRestoreInventoryBackupData() noexcept;

  private:
    /**
     * @brief API to check if inventory backup path has data
     *
     * @return true if inventory backup data is found, false otherwise
     */
    inline bool checkInventoryBackupPath() noexcept;

    /**
     * @brief API to restore inventory data from backup file path to inventory
     * persisted path
     *
     * @return true if the restoration is successful, false otherwise
     */
    inline bool restoreInventoryBackupData() noexcept;

    /**
     * @brief API to clear inventory backup data from backup file path
     *
     * @return true if backup data has been cleared, false otherwise
     */
    inline bool clearInventoryBackupData() noexcept;

    /**
     * @brief API to restart inventory manager service
     *
     * @return true if inventory manager service is successfully restarted,
     * false otherwise
     */
    inline bool restartInventoryManagerService() noexcept;

    /* Members */
    std::string m_inventoryManagerServiceName{vpd::constants::pimServiceName};
    std::filesystem::path m_inventoryBackupPath{vpd::constants::pimBackupPath};

    std::shared_ptr<vpd::Logger> m_logger{nullptr};
};
