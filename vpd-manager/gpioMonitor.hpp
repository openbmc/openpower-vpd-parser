#pragma once
#include "types.hpp"

#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

namespace openpower
{
namespace vpd
{
namespace manager
{
/** @class GpioEventHandler
 *  @brief Responsible for catching the event and handle it.
 *         This keeps checking for the FRU's presence.
 *         If any attachment or de-attachment found, it enables/disables that
 * fru's output gpio and bind/unbind the driver, respectively.
 */
class GpioEventHandler
{
  public:
    GpioEventHandler() = default;
    ~GpioEventHandler() = default;
    GpioEventHandler(const GpioEventHandler&) = default;
    GpioEventHandler& operator=(const GpioEventHandler&) = delete;
    GpioEventHandler(GpioEventHandler&&) = delete;
    GpioEventHandler& operator=(GpioEventHandler&&) = delete;

    GpioEventHandler(std::string& presPin, types::Byte& presValue,
                     std::string& outPin, types::Byte& outValue,
                     std::string& devAddr, std::string& driver,
                     std::string& bus, std::string& objPath,
                     std::shared_ptr<boost::asio::io_context>& ioCon) :
        presencePin(presPin),
        presenceValue(presValue), outputPin(outPin), outputValue(outValue),
        devNameAddr(devAddr), driverType(driver), busType(bus),
        objectPath(objPath)
    {
        doEventAndTimerSetup(ioCon);
    }

  private:
    /** @brief GPIO informations to get parsed from vpd json*/

    // gpio pin indicates presence/absence of fru
    const std::string presencePin;
    // value which means fru is present
    const types::Byte presenceValue;
    // gpio pin to enable If fru is present
    const std::string outputPin;
    // Value to set, to enable the output pin
    const types::Byte outputValue;

    // FRU address on bus
    const std::string devNameAddr;
    // Driver type
    const std::string driverType;
    // Bus type
    const std::string busType;
    // object path of FRU
    const std::string objectPath;

    /** Preserves the GPIO pin value to compare it next time. Default init by
     *  false*/
    bool prevPresPinValue = false;

    /** @brief This is a helper function to read the
     *        current value of Presence GPIO
     *
     *  @returns The GPIO value
     */
    bool getPresencePinValue();

    /** @brief This function will toggle the output gpio as per the presence
     *         state of fru.
     */
    void toggleGpio();

    /** @brief This function checks for fru's presence pin and detects change of
     *         value on that pin, (in case of fru gets attached or de-attached).
     *
     *  @returns true if presence pin value changed
     *           false otherwise
     */
    inline bool hasEventOccurred()
    {
        return getPresencePinValue() != prevPresPinValue;
    }

    /** @brief This function runs a timer , which keeps checking for if an event
     *         happened, if event occured then takes action.
     *
     *  @param[in] ioContext - Pointer to io context object.
     */
    void doEventAndTimerSetup(
        std::shared_ptr<boost::asio::io_context>& ioContext);

    /**
     * @brief Api to handle timer expiry.
     *
     * @param ec - Error code.
     * @param timer - Pointer to timer object.
     */
    void handleTimerExpiry(const boost::system::error_code& ec,
                           std::shared_ptr<boost::asio::steady_timer>& timer);
};

/** @class GpioMonitor
 *  @brief Responsible for initialising the private variables containing gpio
 *         infos. These informations will be fetched from vpd json.
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

    GpioMonitor(nlohmann::json& js,
                std::shared_ptr<boost::asio::io_context>& ioCon) :
        jsonFile(js)
    {
        initGpioInfos(ioCon);
    }

  private:
    // Json file to get the datas
    nlohmann::json& jsonFile;
    // Array of event handlers for all the attachable FRUs
    std::vector<std::shared_ptr<GpioEventHandler>> gpioObjects;

    /** @brief This function will extract the gpio informations from vpd json
     * and store it in GpioEventHandler's private variables
     *
     * @param[in] ioContext - Pointer to io context object.
     */
    void initGpioInfos(std::shared_ptr<boost::asio::io_context>& ioContext);
};

} // namespace manager
} // namespace vpd
} // namespace openpower