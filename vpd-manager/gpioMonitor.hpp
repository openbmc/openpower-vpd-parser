#pragma once

#include <gpiod.h>
#include <boost/asio/io_service.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

namespace openpower
{
namespace vpd
{
namespace gpioMonitor
{

/** @class GpioMonitor
 *  @brief Responsible for catching GPIO state change
 *  condition and starting systemd targets.
 */
class GpioMonitor
{
  public:
    GpioMonitor() = delete;
    ~GpioMonitor() = default;
    GpioMonitor(const GpioMonitor&) = delete;
    GpioMonitor& operator=(const GpioMonitor&) = delete;
    GpioMonitor(GpioMonitor&&) = delete;
    GpioMonitor& operator=(GpioMonitor&&) = delete;

    GpioMonitor(boost::asio::io_service& io) :
        gpioEventDescriptor(io)
    {
        dynamicPresenceDetect();
    };

  private:

    /** @brief GPIO event descriptor */
    boost::asio::posix::stream_descriptor gpioEventDescriptor;

    /**/
    void dynamicPresenceDetect();
    void event_handler();
};

} // namespace gpioMonitor
} // namespace vpd
} // namespace openpower
