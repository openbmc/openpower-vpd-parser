#include "inventory_backup_handler.hpp"

#include "error_codes.hpp"
#include "utility/common_utility.hpp"
#include "utility/dbus_utility.hpp"

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

        const auto l_systemInventoryBackupPath{
            m_inventoryBackupPath /
            std::filesystem::path(vpd::constants::systemVpdInvPath)
                .relative_path()};

        m_logger->logMessage(
            "_SR primPath: " + l_systemInventoryPrimaryPath.string() +
            " backPath: " + l_systemInventoryBackupPath.string());

        if (std::filesystem::exists(l_systemInventoryPrimaryPath) &&
            std::filesystem::is_directory(l_systemInventoryPrimaryPath))
        {
            // TODO: copy all sub directories under system from backup to
            // primary
            auto l_dirIt = std::filesystem::directory_iterator{
                l_systemInventoryBackupPath};

            std::for_each(
                std::filesystem::begin(l_dirIt), std::filesystem::end(l_dirIt),
                [this, l_systemInventoryPrimaryPath = std::as_const(
                           l_systemInventoryPrimaryPath)](const auto& l_entry) {
                    if (l_entry.is_directory())
                    {
                        std::error_code l_ec;

                        m_logger->logMessage(
                            "_SR subDir: " +
                            l_entry.path().filename().string());

                        const auto l_destPath = l_systemInventoryPrimaryPath /
                                                l_entry.path().filename();

                        // try to rename first as it is more efficient
                        std::filesystem::rename(l_entry.path(), l_destPath,
                                                l_ec);

                        if (l_ec)
                        {
                            m_logger->logMessage(
                                "_SR failed to rename, trying brute copy");

                            // rename failed, possibly because destination path
                            // already exists and is not empty
                            //  let's try to remove primary path, copy the data
                            //  in order to overwrite

                            if (std::filesystem::exists(l_destPath))
                            {
                                std::filesystem::remove_all(l_destPath);
                            }

                            // copy with overwrite
                            std::filesystem::copy(
                                l_entry.path(), l_destPath,
                                std::filesystem::copy_options::recursive |
                                    std::filesystem::copy_options::
                                        overwrite_existing);

                            // remove the original after successful copy
                            std::filesystem::remove_all(l_entry.path());
                        }
                    }
                });

            l_rc = true;
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
        auto l_restartInvManager = [this]() -> bool {
            uint16_t l_errCode{0};
            const auto l_cmd =
                "systemctl restart " + m_inventoryManagerServiceName;
            vpd::commonUtility::executeCmd(l_cmd, l_errCode);

            return l_errCode == vpd::constants::VALUE_0;
        };

        // Restart inventory manager service, with a re-try limit of 3 attempts
        constexpr auto l_numRetries{3};

        for (unsigned l_attempt = 0; l_attempt < l_numRetries; ++l_attempt)
        {
            if (l_restartInvManager())
            {
                // restarting inventory manager service is successful, return
                // success
                return true;
            }
        }

        // re-try limit crossed
        m_logger->logMessage(
            "Failed to restart inventory manager service after " +
            std::to_string(l_numRetries) + " attempts");

        // check inventory manager service state and set appropriate error code
        o_errCode =
            vpd::dbusUtility::isServiceRunning(m_inventoryManagerServiceName)
                ? vpd::error_code::SERVICE_RUNNING
                : vpd::error_code::SERVICE_NOT_RUNNING;
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
