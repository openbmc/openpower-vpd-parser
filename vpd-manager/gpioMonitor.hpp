#pragma once

#include <gpiod.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <nlohmann/json.hpp>

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

    GpioMonitor(boost::asio::io_service& io, nlohmann::json& js) :
        gpioEventDescriptor(io), jsonFile(js)
    {
        dynamicPresenceDetect();
    };

  private:

    /** @brief GPIO event descriptor */
    boost::asio::posix::stream_descriptor gpioEventDescriptor;

    nlohmann::json jsonFile;

    /**/
    void dynamicPresenceDetect();
    void event_handler();
};

} // namespace gpioMonitor
} // namespace vpd
} // namespace openpower
