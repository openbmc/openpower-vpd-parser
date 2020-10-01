#pragma once

#include "manager.hpp"

#include <gpiod.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

namespace openpower
{
namespace vpd
{
namespace gpioMonitor
{
using namespace std;
using sdeventplus::ClockId;
using sdeventplus::Event;
constexpr auto clockId = ClockId::RealTime;
using Timer = sdeventplus::utility::Timer<clockId>;

/** @class GpioMonitor
 *  @brief Responsible for collect and init gpio infos
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

    GpioMonitor(nlohmann::json& js, sdbusplus::bus::bus& bus) :
        jsonFile(js), _bus(bus)
    {
        initOppanelGpioInfos();
    };

  private:
    nlohmann::json& jsonFile;
    sdbusplus::bus::bus& _bus;

    /*@brief This is to extract the gpio informations from vpd json and store*/
    void initOppanelGpioInfos();
};

/** @class GpioEventHandler
 *  @brief Responsible for catching event and handle it
 */
class GpioEventHandler
{
  public:
    GpioEventHandler() = delete;
    ~GpioEventHandler() = default;
    GpioEventHandler(const GpioEventHandler&) = delete;
    GpioEventHandler& operator=(const GpioEventHandler&) = delete;
    GpioEventHandler(GpioEventHandler&&) = delete;
    GpioEventHandler& operator=(GpioEventHandler&&) = delete;

    GpioEventHandler(const string& presPin, const uint8_t& presValue,
                     const string& outPin, const uint8_t& outValue,
                     const string& devAddr, const string& driver,
                     const string& bus, sdbusplus::bus::bus& busConn) :
        presencePin(presPin),
        presenceValue(presValue), outputPin(outPin), outputValue(outValue),
        devNameAddr(devAddr), driverType(driver), busType(bus), _dbus(busConn)
    {
        eventHandler();
    };

    /*Preserve the previous GPIO value*/
    bool prevPresenceState;

  private:
    /*op-panel GPIO informations to get initialised from vpd json*/
    string presencePin;
    uint8_t presenceValue;
    string outputPin;
    uint8_t outputValue;

    string devNameAddr;
    string driverType;
    string busType;
    sdbusplus::bus::bus& _dbus;

    /* @brief This function will toggle the gpio as per the presence state.
     */
    void toggleOppanelGpio();

    /* @brief This function checks for an event occurred on GPIO presence pin
     *
     * @returns true if event occurred
     *          false otherwise
     */
    bool eventOccurred();

    /* @brief This is a helper function to read the
     *        current value of Presence GPIO
     *
     * @returns The GPIO value
     */
    bool getCurrentPresenceState();

    /*@brief This function keeps checking for if an event happened
     *       if event occured then take care of action
     */
    void eventHandler();
};

} // namespace gpioMonitor
} // namespace vpd
} // namespace openpower