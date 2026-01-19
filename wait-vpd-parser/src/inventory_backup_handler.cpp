#include "inventory_backup_handler.hpp"

#include "error_codes.hpp"
#include "utility/common_utility.hpp"

bool InventoryBackupHandler::checkInventoryBackupPath(
    uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = 0;
    try
    {
        const auto l_systemInventoryBackupPath{
            m_inventoryBackupPath /
            std::filesystem::path(vpd::constants::systemInvPath)
                .relative_path()};

        if (std::filesystem::is_directory(l_systemInventoryBackupPath) &&
            !std::filesystem::is_empty(l_systemInventoryBackupPath))
        {
            // check number of directories under /system path, as we are only
            // interested in the FRUs VPD which is populated under respective
            // /system/chassis* directories
            auto l_backupDirIt = std::filesystem::directory_iterator{
                l_systemInventoryBackupPath};

            const auto l_numSubDirectories = std::count_if(
                std::filesystem::begin(l_backupDirIt),
                std::filesystem::end(l_backupDirIt),
                [](const auto& l_entry) { return l_entry.is_directory(); });

            l_rc = (l_numSubDirectories != 0);
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = vpd::error_code::STANDARD_EXCEPTION;

        m_logger->logMessage("Failed to check inventory path. Error: " +
                             std::string(l_ex.what()));
    }
    return l_rc;
}

bool InventoryBackupHandler::restoreInventoryBackupData(
    uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = 0;
    try
    {
        if (!checkInventoryBackupPath(o_errCode))
        {
            if (o_errCode)
            {
                throw std::runtime_error(
                    "Failed to check if inventory backup path exists. Error: " +
                    vpd::commonUtility::getErrCodeMsg(o_errCode));
            }
            return l_rc;
        }

        const auto l_systemInventoryPrimaryPath{
            m_inventoryPrimaryPath /
            std::filesystem::path(vpd::constants::systemVpdInvPath)
                .relative_path()};

        if (std::filesystem::is_directory(l_systemInventoryPrimaryPath))
        {
            const auto l_systemInventoryBackupPath{
                m_inventoryBackupPath /
                std::filesystem::path(vpd::constants::systemVpdInvPath)
                    .relative_path()};

            // copy all sub directories under /system from backup path to
            // primary path
            l_rc = syncFiles(l_systemInventoryBackupPath,
                             l_systemInventoryPrimaryPath, o_errCode);
        }
    }
    catch (const std::exception& l_ex)
    {
        const vpd::types::PelInfoTuple l_pelInfo{
            vpd::types::ErrorType::FirmwareError,
            vpd::types::SeverityType::Warning,
            0,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::nullopt};

        m_logger->logMessage("Failed to restore inventory backup data from [" +
                                 m_inventoryBackupPath.string() + "] to [" +
                                 m_inventoryPrimaryPath.string() +
                                 "] Error: " + std::string(l_ex.what()),
                             vpd::PlaceHolder::PEL, &l_pelInfo);

        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

bool InventoryBackupHandler::clearInventoryBackupData(
    uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = 0;
    try
    {
        std::error_code l_ec;

        const auto l_systemInventoryBackupPath{
            m_inventoryBackupPath /
            std::filesystem::path(vpd::constants::systemVpdInvPath)
                .relative_path()};

        std::filesystem::remove_all(l_systemInventoryBackupPath);
    }
    catch (const std::exception& l_ex)
    {
        const vpd::types::PelInfoTuple l_pelInfo{
            vpd::types::ErrorType::FirmwareError,
            vpd::types::SeverityType::Warning,
            0,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::nullopt};

        m_logger->logMessage(
            "Failed to clear inventory backup data from path [" +
                m_inventoryBackupPath.string() +
                "]. Error: " + std::string(l_ex.what()),
            vpd::PlaceHolder::PEL, &l_pelInfo);

        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

bool InventoryBackupHandler::restartInventoryManagerService(
    uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = 0;
    try
    {
        /* TODO:
           1. Restart inventory manager service, with a re-try limit of 3
           attempts
           2. If above step fails, check inventory manager service state
           3. If service is in running state, then log critical PEL and go for
           re-collection. If not running then along with critical PEL, fail this
           service as well.
        */
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage("Failed to restart inventory manager service " +
                             m_inventoryManagerServiceName +
                             ". Error: " + std::string(l_ex.what()));
        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

bool InventoryBackupHandler::syncFiles(const std::filesystem::path& l_src,
                                       const std::filesystem::path& l_dest,
                                       uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = vpd::constants::VALUE_0;
    try
    {
        // use rsync command to sync files
        // -a for archive mode to preserve permissions
        // --delete to make the destination an exact mirror of the source
        // --include flag ensures all directories (and their contents) are
        // selected for sync
        // --exclude flag ensures any file located in the root of the source
        // directory is excluded
        const auto l_cmd = "rsync -a --delete --include='*/' --exclude='/*' " +
                           l_src.string() + "/ " + l_dest.string();

        m_logger->logMessage("_SR executing cmd: \"" + l_cmd + "\"");

        vpd::commonUtility::executeCmd(l_cmd, o_errCode);
        l_rc = (vpd::constants::VALUE_0 == o_errCode);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to sync files from [" + l_src.string() + "] to [" +
            l_dest.string() + "]. Error: " + l_ex.what());

        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }

    return l_rc;
}
