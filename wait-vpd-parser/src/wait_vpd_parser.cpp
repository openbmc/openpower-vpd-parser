#include "config.h"

#include "constants.hpp"
#include "logger.hpp"
#include "utility/dbus_utility.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <thread>

int main(int argc, char** argv)
{
    CLI::App l_app{"Wait VPD parser app "};

    // default retry limit and sleep duration values
    unsigned l_retryLimit{100};
    unsigned l_sleepTimeInSeconds{2};

    l_app.add_option("--retryLimit, -r", l_retryLimit, "Retry limit");
    l_app.add_option("--sleepDurationInSecs, -s", l_sleepTimeInSeconds,
                     "Sleep time in seconds between each retry");

    CLI11_PARSE(l_app, argc, argv);

    try
    {
        vpd::logging::logMessage(
            "Checking every " + std::to_string(l_sleepTimeInSeconds) +
            "s for VPD collection status ....");

        for (unsigned l_retries = l_retryLimit;
             l_retries != vpd::constants::VALUE_0; --l_retries)
        {
            // check every 2 seconds
            std::this_thread::sleep_for(
                std::chrono::seconds(l_sleepTimeInSeconds));

            const auto l_propValue = vpd::dbusUtility::readDbusProperty(
                IFACE, OBJPATH, IFACE, "CollectionStatus");
            if (auto l_val = std::get_if<std::string>(&l_propValue))
            {
                if (*l_val == "Completed")
                {
                    vpd::logging::logMessage("VPD collection is completed");
                    return 0;
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
        return 1;
    }
    catch (const std::exception& l_ex)
    {
        vpd::logging::logMessage(
            "Error in wait VPD parser : " + std::string(l_ex.what()));
    }
    return 1;
}
