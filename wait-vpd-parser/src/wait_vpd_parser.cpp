#include "config.h"

#include "constants.hpp"
#include "logger.hpp"
#include "prime_inventory.hpp"
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
 * @brief API to check if PIM backup path has data
 *
 * @return true if PIM backup data is found, false otherwise
 */
inline bool checkPimBackupPath() noexcept
{
    bool l_rc{false};
    try
    {
        /* TODO:
            Check /var/lib/phosphor-data-sync/bmc_data_bkp/ path to see if it
           has sub directories with PIM data and return true if so.
        */
    }
    catch (const std::exception& l_ex)
    {
        vpd::Logger::getLoggerInstance()->logMessage(
            "Failed to check if PIM backup path has data. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

/**
 * @brief API to restore PIM data from backup file path to PIM persisted path
 *
 * @return true if the restoration was successful, false otherwise
 */
inline bool restorePimBackupData() noexcept
{
    bool l_rc{false};
    try
    {
        /* TODO:
            1. Iterate through directories under
           /var/lib/phosphor-data-sync/bmc_data_bkp/
            2. Extract the object path and interface information
            3. Restore the data to PIM persisted path.
        */
    }
    catch (const std::exception& l_ex)
    {
        vpd::Logger::getLoggerInstance()->logMessage(
            "Failed to restore data to PIM persisted path from backup path. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

/**
 * @brief API to clear PIM backup data from backup file path
 *
 * @return true if backup data has been cleared, false otherwise
 */
inline bool clearPimBackupData() noexcept
{
    bool l_rc{false};
    try
    {
        /* TODO:
           Clear all directories under /var/lib/phosphor-data-sync/bmc_data_bkp/
        */
    }
    catch (const std::exception& l_ex)
    {
        vpd::Logger::getLoggerInstance()->logMessage(
            "Failed to clear data under PIM backup path. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

/**
 * @brief API to restart PIM service
 */
inline bool restartPimService() noexcept
{
    bool l_rc{false};
    try
    {
        /* TODO:
           Restart PIM service
        */
    }
    catch (const std::exception& l_ex)
    {
        vpd::Logger::getLoggerInstance()->logMessage(
            "Failed to restart PIM. Error: " + std::string(l_ex.what()));
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

        if (checkPimBackupPath())
        {
            if (restorePimBackupData())
            {
                // Once backup data is restored, restart PIM service so that the
                // inventory data gets published on D-Bus
                if (!restartPimService())
                {
                    // TODO: log PEL?
                }

                clearPimBackupData();
            }
            else
            {
                // TODO: log PEL?
            }
            exit(EXIT_SUCCESS);
        }
        else
        {
            PrimeInventory l_primeObj;
            l_primeObj.primeSystemBlueprint();

            return collectAllFruVpd()
                       ? checkVpdCollectionStatus(l_retryLimit,
                                                  l_sleepDurationInSeconds)
                       : vpd::constants::VALUE_1;
        }
    }
    catch (const std::exception& l_ex)
    {
        const auto l_logger = vpd::Logger::getLoggerInstance();
        l_logger->logMessage("Exiting from wait-vpd-parser, reason: " +
                             std::string(l_ex.what()));
        return vpd::constants::VALUE_1;
    }
}
