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
        /* TODO:
            Check inventory backup path to see if it
           has sub directories with inventory backup data and return true if so.
        */
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

        /* TODO:
            1. Iterate through directories under
           /var/lib/phosphor-data-sync/bmc_data_bkp/
            2. Extract the object path and interface information
            3. Restore the data to inventory manager persisted path.
        */
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to restore inventory backup data from [" +
                m_inventoryBackupPath.string() + "] to [" +
                m_inventoryPrimaryPath.string() +
                "] Error: " + std::string(l_ex.what()),
            vpd::PlaceHolder::PEL,
            std::optional<const vpd::types::PelInfoTuple>{
                std::in_place, vpd::types::ErrorType::FirmwareError,
                vpd::types::SeverityType::Warning, 0, std::nullopt,
                std::nullopt, std::nullopt, std::nullopt});

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
        /* TODO:
           Clear all directories under inventory backup path
        */
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to clear inventory backup data from path [" +
                m_inventoryBackupPath.string() +
                "]. Error: " + std::string(l_ex.what()),
            vpd::PlaceHolder::PEL,
            std::optional<const vpd::types::PelInfoTuple>{
                std::in_place, vpd::types::ErrorType::FirmwareError,
                vpd::types::SeverityType::Warning, 0, std::nullopt,
                std::nullopt, std::nullopt, std::nullopt});

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
