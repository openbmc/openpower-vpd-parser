#include "logger.hpp"
#include "constants.hpp"
#include "config.h"
#include "utility/dbus_utility.hpp"
#include <chrono>
int main(int argc, char** argv)
{
    try
    {
        constexpr unsigned l_retries{100};
        logging::logMessage("Checking every 2s for VPD collection status ....");
        auto l_start = std::chrono::high_resolution_clock::now();
        do
        {
            //check every 2 seconds
            if((std::chrono::high_resolution_clock::now() - l_start) > std::chrono::seconds(2s))
            {
                const auto l_propValue = vpd::dbusUtility::readDbusProperty(IFACE,OBJPATH,IFACE,"CollectionStatus");
                if(auto l_val = std::get_if<std::string>(&l_propValue))
                {
                    if(*l_val == "Completed")
                    {
                        logging::logMessage("VPD collection is completed");
                        return 0;
                    }
                }
                else
                {
                    logging::logMessage("Invalid value read for CollectionStatus");
                }
                l_start = std::chrono::high_resolution_clock::now();
            }
            logging::logMessage("Waiting for VPD status update. Retries remaining: " + std::to_string(l_retries));
        } while (l_retries != constants::VALUE_0);
        logging::logMessage("Exit wait for VPD services to finish with timeout");
        return 1;
    }
    catch(const std::exception& l_ex)
    {
        logging::logMessage("Error in wait VPD parser : " + std::string_view(l_ex.what()));
    }
    return 1;
}