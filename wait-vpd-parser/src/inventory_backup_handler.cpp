#include "inventory_backup_handler.hpp"

#include "error_codes.hpp"
#include "utility/common_utility.hpp"

inline bool InventoryBackupHandler::checkInventoryBackupPath(
    uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = 0;
    try
    {
        const auto l_systemInventoryBackupPath{
            m_inventoryBackupPath / vpd::constants::systemInvPath};

        if (std::filesystem::exists(l_systemInventoryBackupPath) &&
            std::filesystem::is_directory(l_systemInventoryBackupPath) &&
            !std::filesystem::is_empty(l_systemInventoryBackupPath))
        {
            // check number of directories under system inventory
            auto l_dirIt = std::filesystem::directory_iterator{
                l_systemInventoryBackupPath};

            const auto l_numSubDirectories = std::count_if(
                std::filesystem::begin(l_dirIt), std::filesystem::end(l_dirIt),
                [](const auto& l_entry) { return l_entry.is_directory(); });

            l_rc = (l_numSubDirectories != 0);
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

inline bool InventoryBackupHandler::restoreInventoryBackupData(
    uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = 0;
    try
    {
        /* TODO:
            1. Iterate through directories under
           /var/lib/phosphor-data-sync/bmc_data_bkp/
            2. Extract the object path and interface information
            3. Restore the data to inventory manager persisted path.
        */
        const auto l_systemInventoryPrimaryPath{
            m_inventoryPrimaryPath / vpd::constants::systemVpdInvPath};

        const auto l_systemInventoryBackupPath{
            m_inventoryBackupPath / vpd::constants::systemInvPath};

        if (std::filesystem::exists(l_systemInventoryPrimaryPath) &&
            std::filesystem::is_directory(l_systemInventoryPrimaryPath))
        {
            std::filesystem::rename(l_systemInventoryBackupPath,
                                    l_systemInventoryPrimaryPath);
            l_rc = true;
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

inline bool InventoryBackupHandler::clearInventoryBackupData(
    uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = 0;
    try
    {
        //     auto l_systemInventoryBackupPath{m_inventoryBackupPath};
        //     l_systemInventoryBackupPath +=
        //         std::filesystem::path{vpd::constants::systemInvPath};

        //    std::filesystem::remove_all(l_systemInventoryBackupPath);

        l_rc = true;
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

inline bool InventoryBackupHandler::restartInventoryManagerService(
    uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = 0;
    try
    {
        const auto l_cmd = "systemctl restart " + m_inventoryManagerServiceName;
        vpd::commonUtility::executeCmd(l_cmd, o_errCode);
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

bool InventoryBackupHandler::checkAndRestoreInventoryBackupData() const noexcept
{
    bool l_rc{false};
    uint16_t l_errCode{0};
    try
    {
        l_errCode = 0;
        if (!checkInventoryBackupPath(l_errCode))
        {
            if (l_errCode)
            {
                throw std::runtime_error(
                    "Failed to check if inventory backup path exists. Error: " +
                    vpd::commonUtility::getErrCodeMsg(l_errCode));
            }
            return l_rc;
        }

        if (!restoreInventoryBackupData(l_errCode))
        {
            /* TODO: re-try restoreInventoryBackupData ? */
            throw std::runtime_error(
                "Failed to restore inventory backup data. Error: " +
                vpd::commonUtility::getErrCodeMsg(l_errCode));
        }

        if (!restartInventoryManagerService(l_errCode))
        {
            throw std::runtime_error(
                "Failed to restart inventory manager service. Error: " +
                vpd::commonUtility::getErrCodeMsg(l_errCode));
        }

        if (!clearInventoryBackupData(l_errCode))
        {
            throw std::runtime_error(
                "Failed to clear inventory data. Error: " +
                vpd::commonUtility::getErrCodeMsg(l_errCode));
        }

        l_rc = true;
    }
    catch (const std::exception& l_ex)
    {
        std::string l_errMsg{
            "Failed to check and restore inventory backup data. Error: " +
            std::string(l_ex.what())};

        m_logger->logMessage(l_errMsg);

        // TODO: log PEL

        l_rc = false;
    }
    return l_rc;
}
