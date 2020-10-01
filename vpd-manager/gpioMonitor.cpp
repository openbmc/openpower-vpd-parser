#include "config.h"

#include "gpioMonitor.hpp"
#include <iostream>

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
    //open chip get line
    gpiod_chip* chip = gpiod_chip_open("gpiochip0");
    if(!chip)
    {
        cout<<"Error: CHIP NULL \n";
        // log an error
        // break
    }

    cout<<"Chip opened, getting line\n";
    // @brief GPIO line
    gpiod_line* gpioLine = NULL;
    gpioLine = gpiod_chip_get_line(chip, 7);
//    gpioLine = gpiod::find_line(presencePinName);
    if( !gpioLine)
    {
        cout<<"Error: gpioLine NULL \n";
        // error break
    }

    cout<<"Got gpioLine\n";
    // Request an event to monitor for respected gpio line
    gpiod_line_request_config config{
        "gpio_event_monitor", GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES, 0
    };
    if (gpiod_line_request(gpioLine, &config, 0) < 0)
    { 
        //log error,
        cout<<"gpiod_line_request Failed\n";
        return;
    }
    cout<<"Got gpiod_line_request\n";

    //get FD
    int gpioLineFd = gpiod_line_event_get_fd(gpioLine);
    if (gpioLineFd < 0)
    {
        //log error;
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

    //Event handler
    gpiod_line_event gpioLineEvent;
    if (gpiod_line_event_read_fd(gpioEventDescriptor.native_handle(),
                                 &gpioLineEvent) < 0)
    {
        //log error;
        cout<<"gpiod_line_event_read_fd Failed\n";
        return;
    }

    // Action: check if present. If yes, then further action
    
    //continue to wait. Call wait event.

}

} // namespace gpioMonitor
} // namespace vpd
} // namespace openpower
