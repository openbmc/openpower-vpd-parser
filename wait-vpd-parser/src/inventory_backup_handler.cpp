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
            auto l_backupDirIt = std::filesystem::directory_iterator{
                l_systemInventoryBackupPath};

            // vector to hold a list of sub directories under /system for which
            // restoration failed
            using FailedPathList = std::vector<std::filesystem::path>;
            FailedPathList l_failedPaths;

            std::for_each(std::filesystem::begin(l_backupDirIt),
                          std::filesystem::end(l_backupDirIt),
                          [this,
                           l_systemInventoryPrimaryPath =
                               std::as_const(l_systemInventoryPrimaryPath),
                           &l_failedPaths](const auto& l_entry) {
                              if (l_entry.is_directory())
                              {
                                  uint16_t l_errCode{vpd::constants::VALUE_0};

                                  if (!moveFiles(l_entry.path(),
                                                 l_systemInventoryPrimaryPath /
                                                     l_entry.path().filename(),
                                                 l_errCode))
                                  {
                                      l_failedPaths.emplace_back(
                                          l_entry.path().filename());
                                  }
                              }
                          });

            // check if there are any paths for which restoration failed, if yes
            // log a PEL
            if (!l_failedPaths.empty())
            {
                std::string l_formattedFailedPaths{"Failed paths: "};

                // create a string representation of the failed path list
                for (const auto& l_path : l_failedPaths)
                {
                    l_formattedFailedPaths +=
                        std::format("[{}],", l_path.string());
                }

                m_logger->logMessage(
                    "Failed to restore inventory data for some chassis. Check user data for details",
                    vpd::PlaceHolder::PEL,
                    vpd::types::PelInfoTuple{
                        vpd::types::ErrorType::FirmwareError,
                        vpd::types::SeverityType::Warning, 0,
                        l_formattedFailedPaths, std::nullopt, std::nullopt,
                        std::nullopt});
            }
            // We have logged a PEL for the paths(chassis) which we have failed
            // to move. Consider restoration is successful even if have failed
            // to move some of the paths (chassis) under /system, so that
            // inventory manager exposes atleast some of the chassis.
            l_rc = true;
        }
        else
        {
            o_errCode = vpd::error_code::INVALID_INVENTORY_PATH;
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to restore inventory backup data from [" +
                m_inventoryBackupPath.string() + "] to [" +
                m_inventoryPrimaryPath.string() +
                "] Error: " + std::string(l_ex.what()),
            vpd::PlaceHolder::PEL,
            vpd::types::PelInfoTuple{vpd::types::ErrorType::FirmwareError,
                                     vpd::types::SeverityType::Warning, 0,
                                     std::nullopt, std::nullopt, std::nullopt,
                                     std::nullopt});

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
        const auto l_systemInventoryBackupPath{
            m_inventoryBackupPath /
            std::filesystem::path(vpd::constants::systemVpdInvPath)
                .relative_path()};

        std::filesystem::remove_all(l_systemInventoryBackupPath);

        l_rc = true;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to clear inventory backup data from path [" +
                m_inventoryBackupPath.string() +
                "]. Error: " + std::string(l_ex.what()),
            vpd::PlaceHolder::PEL,
            vpd::types::PelInfoTuple{vpd::types::ErrorType::FirmwareError,
                                     vpd::types::SeverityType::Warning, 0,
                                     std::nullopt, std::nullopt, std::nullopt,
                                     std::nullopt});

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

bool InventoryBackupHandler::moveFiles(const std::filesystem::path& l_src,
                                       const std::filesystem::path& l_dest,
                                       uint16_t& o_errCode) const noexcept
{
    bool l_rc{false};
    o_errCode = vpd::constants::VALUE_0;
    try
    {
        std::error_code l_ec;

        // try to rename first as it is more efficient
        std::filesystem::rename(l_src, l_dest, l_ec);

        if (l_ec == std::errc::directory_not_empty)
        {
            // rename failed, because destination path
            // already exists and is not empty
            //  let's try to copy the data while overwriting

            std::filesystem::copy(
                l_src, l_dest,
                std::filesystem::copy_options::recursive |
                    std::filesystem::copy_options::overwrite_existing);

            // remove the original after successful copy
            if (static_cast<std::uintmax_t>(-1) ==
                std::filesystem::remove_all(l_src, l_ec))
            {
                m_logger->logMessage(
                    "Failed to remove file [" + l_src.string() +
                        "]. Error: " + l_ec.message(),
                    vpd::PlaceHolder::COLLECTION);
            }
        }
        else if (l_ec)
        {
            // for all other errors, we need to throw so
            // that it fails the operation
            throw std::runtime_error(l_ec.message());
        }

        l_rc = true;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to move files from [" + l_src.string() + "] to [" +
                l_dest.string() + "]. Error: " + l_ex.what(),
            vpd::PlaceHolder::COLLECTION);

        o_errCode = vpd::error_code::STANDARD_EXCEPTION;
    }

    return l_rc;
}
