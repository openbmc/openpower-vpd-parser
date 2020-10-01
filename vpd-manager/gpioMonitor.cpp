#include "config.h"
#include <nlohmann/json.hpp>
#include "gpioMonitor.hpp"
#include <iostream>
#include "utils.hpp"

using namespace std;
using namespace openpower::vpd::gpioMonitor;

namespace openpower
{
namespace vpd
{
namespace gpioMonitor
{

void  GpioMonitor::dynamicPresenceDetect()
{
    cout<<"dynamicPresenceDetect ENter \n";
/************Attempt1- get data from vpd JSon*************/
    string presencePinName= "";
    gpiod::line presenceLine;
    for (const auto& eachFRU : jsonFile["frus"].items())
    {
        for (const auto& eachInventory : eachFRU.value())
        {
            if (eachInventory.find("preAction") != eachInventory.end())
            {
                // Get the pin No and polarity to know the "presence"
                for (const auto& presStatus : eachInventory["presence"].items())
                {
                    if (presStatus.key() == "pin")
                    {
                        presencePinName = presStatus.value();
                    }
                    else if (presStatus.key() == "polarity")
                    {
                        if (presStatus.value() == "ACTIVE_HIGH")
                        {
                            //polarity = 1;
                        }
                    }
                }

                //uint8_t gpioData = 0;
                presenceLine = gpiod::find_line(presencePinName);
                cout<<"Got gpioLine\n";
                if (!presenceLine)
                {
                    cout << "configGPIO: couldn't find presence line:"
                         << presencePinName << " on GPIO, for Inventory path- "
                         << eachInventory["inventoryPath"] << ". Skipping...\n";
                    continue;
                }

                presenceLine.request({"Op-panel presence line",
                                     gpiod::line_request::EVENT_BOTH_EDGES, 0});
                cout<<"Got gpiod::line_request\n";

                break;
            }
        }
        // break on 1st for loop If got the line
    }
    //get FD
    int gpioLineFd = presenceLine.event_get_fd();


/********Attempt:2 - Try to open direct path****************************/
/*
    //open chip get line
    gpiod::chip chip("/sys/class/gpio/gpiochip600/device");
    if(!chip)
    {
        cout<<"Error: CHIP NULL \n";
        return;
    }
cout<<"Got chip initialised\n";

    gpiod::line  gpioLine = chip.get_line( 2);
    if( !gpioLine)
    {
        cout<<"Error: gpioLine NULL \n";
        return;
    }

    cout<<"Got gpioLine\n";
    // Request an event to monitor for respected gpio line
    gpiod::line_request config{
        "gpio_event_monitor",  gpiod::line_request::EVENT_BOTH_EDGES, 0
    };
    gpioLine.request(config);
 
    cout<<"Got gpiod_line_request\n";

    //get FD
    int gpioLineFd = gpioLine.event_get_fd();
*/

    if (gpioLineFd < 0)
    {
        cout<<"gpiod_line_event_get_fd Failed \n";
        return;
    }
    cout<<"Got gpioLineFd \n";

    // Assign line fd to descriptor for monitoring
    gpioEventDescriptor.assign(gpioLineFd);
cout<<"gpioEventDescriptor assigned FD\n";

    // Schedule a wait event
    gpioEventDescriptor.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [this](const boost::system::error_code& ec) {
        if (ec)
        {
            cout<<"Enter event_handler: error \n";
            return;
        }

        event_handler();
        });


cout<<"gpioEventDescriptor.async_wait DOne\n";

}

void  GpioMonitor::event_handler()
{
    /*
    if (ec)
    {
        cout<<"Enter event_handler: error \n";
        return;
    }*/

cout<<"Enter event_handler: processing\n";
/* This code "gpiod_line_event_read_fd" works with gpiod.h
 * change it to use with gpiod.hpp*/
/*
    //Event handler
    gpiod_line_event gpioLineEvent;
    if (gpiod_line_event_read_fd(gpioEventDescriptor.native_handle(),
                                 &gpioLineEvent) < 0)
    {
        //log error;
        cout<<"gpiod_line_event_read_fd Failed\n";
        return;
    }
*/
    // Action: check if present. If yes, then further action
    // continue to wait? Call wait event.
}

} // namespace gpioMonitor
} // namespace vpd
} // namespace openpower
