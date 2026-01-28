#include "config.h"

#include <exception>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    try
    {
        // getAll the FRUs From Json
        // process each FRU path
        /*
        for(eachFrus)
            if (SystemVPD)
            continue;    //skip it
            Else
            {
            -delete it from PIM’s persisted path
        (/var/lib/phosphor-data-sync/bmc_data_bkp/) -call dbus method to
        deleteObject for this_object
            }
        }//for loop
        */
        /*
        - Restart the PIM service
        - set the status for Sysytem VPD collection as “collected”
        */
    }
    catch ([[maybe_unused]] const std::exception& l_ex)
    {}
}
