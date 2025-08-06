#include "config.h"

#include "constants.hpp"
#include "logger.hpp"
#include "utility/dbus_utility.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <thread>

/**
 * @brief API to check for VPD collection status
 *
 * This API checks for VPD manager collection status by reading the
 * "CollectionStatus" property exposed by vpd-manager on Dbus. The read logic
 * uses a retry loop with a specific number of retries with specific sleep time
 * between each retry.
 *
 * @param[in] i_retryLimit - Maximum number of retries
 * @param[in] i_sleepTimeInSeconds - Sleep time in seconds between each retry
 *
 * @return If "CollectionStatus" property is "Completed", returns 0, otherwise
 * returns 1.
 */
int checkVpdCollectionCompletion(const unsigned i_retryLimit,
                                 const unsigned i_sleepTimeInSeconds) noexcept
{
    try
    {
        vpd::logging::logMessage(
            "Checking every " + std::to_string(i_sleepTimeInSeconds) +
            "s for VPD collection status ....");

        for (unsigned l_retries = i_retryLimit;
             l_retries != vpd::constants::VALUE_0; --l_retries)
        {
            // check every 2 seconds
            std::this_thread::sleep_for(
                std::chrono::seconds(i_sleepTimeInSeconds));

            const auto l_propValue = vpd::dbusUtility::readDbusProperty(
                IFACE, OBJPATH, IFACE, "CollectionStatus");
            if (auto l_val = std::get_if<std::string>(&l_propValue))
            {
                if (*l_val == "Completed")
                {
                    vpd::logging::logMessage("VPD collection is completed");
                    return vpd::constants::VALUE_0;
                }
            }
            else
            {
                vpd::logging::logMessage(
                    "Invalid value read for CollectionStatus");
            }

            vpd::logging::logMessage(
                "Waiting for VPD status update. Retries remaining: " +
                std::to_string(l_retries));
        }

        vpd::logging::logMessage(
            "Exit wait for VPD services to finish with timeout");
    }
    catch (const std::exception& l_ex)
    {
        vpd::logging::logMessage(
            "Error while checking VPD collection status: " +
            std::string(l_ex.what()));
    }

    return vpd::constants::VALUE_1;
}

int main(int argc, char** argv)
{
    CLI::App l_app{"Wait VPD parser app"};

    // default retry limit and sleep duration values
    unsigned l_retryLimit{100};
    unsigned l_sleepTimeInSeconds{2};

    l_app.add_option("--retryLimit, -r", l_retryLimit, "Retry limit");
    l_app.add_option("--sleepDurationInSecs, -s", l_sleepTimeInSeconds,
                     "Sleep time in seconds between each retry");

    CLI11_PARSE(l_app, argc, argv);

    int l_rc{vpd::constants::VALUE_0};
    try
    {
        l_rc = checkVpdCollectionCompletion(l_retryLimit, l_sleepTimeInSeconds);
    }
    catch (const std::exception& l_ex)
    {
        vpd::logging::logMessage(
            "Error in wait VPD parser : " + std::string(l_ex.what()));
    }
    return l_rc;
}
