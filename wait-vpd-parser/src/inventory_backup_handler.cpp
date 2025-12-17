#include "inventory_backup_handler.hpp"

inline bool InventoryBackupHandler::checkInventoryBackupPath() noexcept
{
    bool l_rc{false};
    try
    {
        /* TODO:
            Check inventory backup path to see if it
           has sub directories with inventory backup data and return true if so.
        */
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to check if inventory manager backup path has data. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

inline bool InventoryBackupHandler::restoreInventoryBackupData() noexcept
{
    bool l_rc{false};
    try
    {
        /* TODO:
            1. Iterate through directories under
           /var/lib/phosphor-data-sync/bmc_data_bkp/
            2. Extract the object path and interface information
            3. Restore the data to inventory manager persisted path.
        */

        // create sym link??
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to restore data to inventory manager persisted path from backup path. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

inline bool InventoryBackupHandler::clearInventoryBackupData() noexcept
{
    bool l_rc{false};
    try
    {
        /* TODO:
           Clear all directories under inventory backup path
        */
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to clear data under inventory manager backup path. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

inline bool InventoryBackupHandler::restartInventoryManagerService() noexcept
{
    bool l_rc{false};
    try
    {
        /* TODO:
           Restart inventory manager service
        */
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage("Failed to restart inventory manager. Error: " +
                             std::string(l_ex.what()));
    }
    return l_rc;
}

bool InventoryBackupHandler::checkAndRestoreInventoryBackupData() noexcept
{
    bool l_rc{false};
    try
    {
        if (checkInventoryBackupPath())
        {
            if (restoreInventoryBackupData())
            {
                if (!restartInventoryManagerService())
                {
                    /* TODO: log PEL?*/
                }

                clearInventoryBackupData();
            }
            else
            {
                /* TODO: re-try restoreInventoryBackupData ? */
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to check and restore inventory backup data. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}
