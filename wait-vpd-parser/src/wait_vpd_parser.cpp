#include "config.h"

#include "collection_orchestrator.hpp"
#include "constants.hpp"
#include "inventory_backup_handler.hpp"
#include "logger.hpp"
#include "prime_inventory.hpp"
#include "utility/common_utility.hpp"
#include "utility/dbus_utility.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <thread>

constexpr auto active = "active";
constexpr auto passive = "passive";

/**
 * @brief API to handle inventory backup data
 *
 * This API handles inventory backup data. It checks if there is any inventory
 * backup data and restores it if so. It also restarts the inventory manager
 * service so that the restored data is reflected on D-Bus, and then clears the
 * backup data.
 *
 * @return true if inventory backup data is found and restored successfully, and
 * inventory manager service is successfully restarted. It returns false if
 * there is any error encountered while restoring backup data or restarting
 * inventory manager service
 *
 * @throw std::runtime_error
 */
bool checkAndHandleInventoryBackup()
{
    bool l_rc{false};
    uint16_t l_errCode{0};
    auto l_logger = vpd::Logger::getLoggerInstance();

    InventoryBackupHandler l_inventoryBackupHandler{
        vpd::constants::pimServiceName, vpd::constants::pimPrimaryPath,
        vpd::constants::pimBackupPath};

    const auto l_restoreInventoryStartTime = std::chrono::steady_clock::now();
    if (!l_inventoryBackupHandler.restoreInventoryBackupData(l_errCode))
    {
        if (l_errCode)
        {
            l_logger->logMessage(
                "Failed to restore inventory backup data. Error: " +
                vpd::commonUtility::getErrCodeMsg(l_errCode));
        }
        return l_rc;
    }

    l_logger->logMessage(
        "Time taken to restore inventory backup data: " +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - l_restoreInventoryStartTime)
                .count()) +
        "ms");

    // restart the inventory manager service so that the new inventory
    // data is reflected on D-Bus
    if (l_inventoryBackupHandler.restartInventoryManagerService(l_errCode))
    {
        // clear the backup inventory data
        l_inventoryBackupHandler.clearInventoryBackupData(l_errCode);

        // inventory backup restoration and service restart are
        // successful, so return success from here as FRU VPD collection
        // is not needed
        l_rc = true;
    }
    else
    {
        // Restarting the inventory manager service has failed,
        // and error code indicates inventory manager service is not running,
        // throw an exception so that wait-vpd-parsers service also fails.
        // if error code indicates inventory service is running, return false to
        // indicate to caller to proceed for FRU VPD collection.
        if (l_errCode == vpd::error_code::SERVICE_NOT_RUNNING)
        {
            throw std::runtime_error(
                "Failed to restart inventory manager service after restoring backup inventory data. Failing this service");
        }
    }
    return l_rc;
}

/**
 * @brief API to delete VPD for all FRUs.
 *
 * This API deletes VPD for all FRUs by calling Dbus API
 * "DeleteAllFRUVPD" exposed by vpd-manager
 *
 * @return - On success returns true, otherwise returns false
 */
inline int deleteAllFruVpd() noexcept
{
    bool l_rc{true};
    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method =
            l_bus.new_method_call(BUSNAME, OBJPATH, IFACE, "DeleteAllFRUVPD");

        l_bus.call_noreply(l_method);
    }
    catch (const std::exception& l_ex)
    {
        auto l_logger = vpd::Logger::getLoggerInstance();
        l_logger->logMessage("Failed to trigger delete all FRU VPD. Error: " +
                             std::string(l_ex.what()));
        l_rc = false;
    }
    return l_rc;
}

int main(int argc, char** argv)
{
    try
    {
        CLI::App l_app{"Wait VPD parser app"};

        // default collection status timeout in seconds
        unsigned l_collectionStatusTimeoutSecs{1200};

        // The BMC role can be either active or passive. By default the BMC is
        // considered as active.
        std::string l_role{active};

        l_app.add_option("--collectionStatusTimeout, -s",
                         l_collectionStatusTimeoutSecs,
                         "VPD collection status timeout");
        l_app.add_option("--role, -b", l_role, "BMC role");

        CLI11_PARSE(l_app, argc, argv);

        if (l_role == passive)
        {
            return deleteAllFruVpd()
                       ? !(vpd::dbusUtility::writeDbusProperty(
                             BUSNAME, OBJPATH,
                             vpd::constants::vpdCollectionInterface, "Status",
                             vpd::constants::vpdCollectionCompleted))
                       : !(vpd::dbusUtility::writeDbusProperty(
                             BUSNAME, OBJPATH,
                             vpd::constants::vpdCollectionInterface, "Status",
                             vpd::constants::vpdCollectionFailed));
        }

        // check and see if there is any inventory backup data. If it's there
        // restore the data.
        if (checkAndHandleInventoryBackup())
        {
            // backup data is found and restored successfully, return success
            // here as we don't need to go for FRU VPD collection.
            return vpd::constants::VALUE_0;
        }

        PrimeInventory l_primeObj;
        l_primeObj.primeSystemBlueprint();

        // trigger FRU VPD collection and check for status
        CollectionOrchestrator l_collectionOrchestrator{
            BUSNAME, OBJPATH, IFACE, "CollectAllFRUVPD",
            l_collectionStatusTimeoutSecs};

        l_collectionOrchestrator.triggerFruVpdCollectionAndCheckStatus();

        return vpd::constants::VALUE_0;
    }
    catch (const std::exception& l_ex)
    {
        const auto l_logger = vpd::Logger::getLoggerInstance();
        l_logger->logMessage("Exiting from wait-vpd-parser, reason: " +
                             std::string(l_ex.what()));
        return vpd::constants::VALUE_1;
    }
}
