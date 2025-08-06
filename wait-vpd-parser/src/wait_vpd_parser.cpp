#include "config.h"

#include "constants.hpp"
#include "logger.hpp"
#include "utility/dbus_utility.hpp"

#include <chrono>
int main(int, char**)
{
    try
    {
        constexpr unsigned l_retries{100};
        vpd::logging::logMessage(
            "Checking every 2s for VPD collection status ....");
        auto l_start = std::chrono::high_resolution_clock::now();
        do
        {
            // check every 2 seconds
            if ((std::chrono::high_resolution_clock::now() - l_start) >
                std::chrono::seconds(2))
            {
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
                l_start = std::chrono::high_resolution_clock::now();
            }
            vpd::logging::logMessage(
                "Waiting for VPD status update. Retries remaining: " +
                std::to_string(l_retries));
        } while (l_retries != vpd::constants::VALUE_0);
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
