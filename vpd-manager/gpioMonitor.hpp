#pragma once

#include "manager.hpp"

#include <nlohmann/json.hpp>
#include <sdeventplus/event.hpp>

namespace openpower
{
namespace vpd
{
namespace manager
{
namespace gpiomonitor
{
/** @class GpioEventHandler
 *  @brief Responsible for catching the event and handle it.
 *         This keeps checking for the fru's attachment or de-attachment.
 *         If any of the above event found, it enables/disables that fru's
 *         output gpio and bind/unbind the driver, respectively.
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

    GpioEventHandler(std::string& presPin, Byte& presValue, std::string& outPin,
                     Byte& outValue, std::string& devAddr, std::string& driver,
                     std::string& bus, sdeventplus::Event& event) :
        presencePin(presPin),
        presenceValue(presValue), outputPin(outPin), outputValue(outValue),
        devNameAddr(devAddr), driverType(driver), busType(bus)
    {
        doEventAndTimerSetup(event);
    }

  private:
    /*GPIO informations to get initialised from vpd json*/

    /*gpio pin indicates presence/absence of fru*/
    const std::string presencePin;
    /*value which means fru is present*/
    const Byte presenceValue;
    /*gpio pin to enable If fru is present*/
    const std::string outputPin;
    /*Value to set, to enable the output pin*/
    const Byte outputValue;

    /*FRU address on bus*/
    const std::string devNameAddr;
    /*Driver type*/
    const std::string driverType;
    /*Bus type*/
    const std::string busType;

    /*Preserves the GPIO pin value to compare it next time. Default init by
     * false*/
    bool prevPresPinValue = false;

    /* @brief This is a helper function to read the
     *        current value of Presence GPIO
     *
     * @returns The GPIO value
     */
    bool getPresencePinValue();

    /* @brief This function will toggle the output gpio as per the presence
     * state of fru.
     */
    void toggleGpio();

    /* @brief This function checks for fru's presence pin and detects change of
     * value on that pin, (in case of fru gets attached or de-attached).
     *
     * @returns true if presence pin value changed
     *          false otherwise
     */
    bool hasEventOccurred();

    /* @brief This function runs a timer , which keeps checking for if an event
     *        happened, if event occured then takes action.
     * @param[in] timer- Shared pointer of Timer to do event setup for each
     * object.
     * @param[in] event- Event which needs to be tagged with the timer.
     */
    void doEventAndTimerSetup(sdeventplus::Event& event);
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

    GpioMonitor(nlohmann::json& js, sdeventplus::Event& event) : jsonFile(js)
    {
        initGpioInfos(event);
    }

  private:
    nlohmann::json& jsonFile;
    std::vector<std::shared_ptr<GpioEventHandler>> gpioObjects;

    /* @brief This function will extract the gpio informations from vpd json and
     *        store it in GpioEventHandler's private variables
     * @param[in] gpioObj - shared object to initialise it's data and it's Timer
     * setup
     * @param[in] requestedGpioPin - Which GPIO's informations need to be stored
     * @param[in] timer - shared object of timer to do the event setup
     * @param[in] event - event to be tagged with timer.
     */
    void initGpioInfos(sdeventplus::Event& event);
};

} // namespace gpiomonitor
} // namespace manager
} // namespace vpd
} // namespace openpower