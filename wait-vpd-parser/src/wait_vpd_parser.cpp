#include "config.h"

#include "constants.hpp"
#include "inventory_backup_handler.hpp"
#include "logger.hpp"
#include "prime_inventory.hpp"
#include "utility/common_utility.hpp"
#include "utility/dbus_utility.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <thread>

/**
 * @brief API to check for VPD collection status
 *
 * This API checks for VPD manager collection status by reading the
 * collection "Status" property exposed by vpd-manager on Dbus. The read logic
 * uses a retry loop with a specific number of retries with specific sleep time
 * between each retry.
 *
 * @param[in] i_retryLimit - Maximum number of retries
 * @param[in] i_sleepDurationInSeconds - Sleep time in seconds between each
 * retry
 *
 * @return If "CollectionStatus" property is "Completed", returns 0, otherwise
 * returns 1.
 */
int checkVpdCollectionStatus(const unsigned i_retryLimit,
                             const unsigned i_sleepDurationInSeconds) noexcept
{
    auto l_logger = vpd::Logger::getLoggerInstance();

    try
    {
        l_logger->logMessage(
            "Checking every " + std::to_string(i_sleepDurationInSeconds) +
            "s for VPD collection status ....");

        for (unsigned l_retries = i_retryLimit;
             l_retries != vpd::constants::VALUE_0; --l_retries)
        {
            // check at specified time interval
            std::this_thread::sleep_for(
                std::chrono::seconds(i_sleepDurationInSeconds));

            const auto l_propValue = vpd::dbusUtility::readDbusProperty(
                IFACE, OBJPATH, vpd::constants::vpdCollectionInterface,
                "Status");

            if (auto l_val = std::get_if<std::string>(&l_propValue))
            {
                if (*l_val == vpd::constants::vpdCollectionCompleted)
                {
                    l_logger->logMessage("VPD collection is completed");
                    return vpd::constants::VALUE_0;
                }
            }

            l_logger->logMessage(
                "Waiting for VPD status update. Retries remaining: " +
                std::to_string(l_retries));
        }

        l_logger->logMessage(
            "Exit wait for VPD services to finish with timeout");
    }
    catch (const std::exception& l_ex)
    {
        l_logger->logMessage("Error while checking VPD collection status: " +
                             std::string(l_ex.what()));
    }

    return vpd::constants::VALUE_1;
}

/**
 * @brief API to trigger VPD collection for all FRUs.
 *
 * This API triggers VPD collection for all FRUs by calling Dbus API
 * "CollectAllFRUVPD" exposed by vpd-manager
 *
 * @return - On success returns true, otherwise returns false
 */
inline bool collectAllFruVpd() noexcept
{
    bool l_rc{true};
    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method =
            l_bus.new_method_call(IFACE, OBJPATH, IFACE, "CollectAllFRUVPD");

        l_bus.call_noreply(l_method);
    }
    catch (const std::exception& l_ex)
    {
        auto l_logger = vpd::Logger::getLoggerInstance();
        l_logger->logMessage(
            "Failed to trigger all FRU VPD collection. Error: " +
            std::string(l_ex.what()));
        l_rc = false;
    }
    return l_rc;
}

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

    if (!l_inventoryBackupHandler.restoreInventoryBackupData(l_errCode))
    {
        return l_rc;
    }

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
        // TODO: Restarting the inventory manager service has failed,
        // check error code to determine whether to go for VPD
        //  collection or to fail this service
    }
    return l_rc;
}

int main(int argc, char** argv)
{
    try
    {
        CLI::App l_app{"Wait VPD parser app"};

        // default retry limit and sleep duration values
        unsigned l_retryLimit{100};
        unsigned l_sleepDurationInSeconds{2};

        l_app.add_option("--retryLimit, -r", l_retryLimit, "Retry limit");
        l_app.add_option("--sleepDurationInSeconds, -s",
                         l_sleepDurationInSeconds,
                         "Sleep duration in seconds between each retry");

        CLI11_PARSE(l_app, argc, argv);

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

        return collectAllFruVpd()
                   ? checkVpdCollectionStatus(l_retryLimit,
                                              l_sleepDurationInSeconds)
                   : vpd::constants::VALUE_1;
    }
    catch (const std::exception& l_ex)
    {
        const auto l_logger = vpd::Logger::getLoggerInstance();
        l_logger->logMessage("Exiting from wait-vpd-parser, reason: " +
                             std::string(l_ex.what()));
        return vpd::constants::VALUE_1;
    }
}
