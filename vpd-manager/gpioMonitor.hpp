#pragma once

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

    GpioMonitor(nlohmann::json& js) : jsonFile(js)
    {
        initOppanelGpioInfos();
    };

  private:
    nlohmann::json jsonFile;

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

    GpioEventHandler(string& presPin, uint8_t& presPolarity, string& outPin,
                     uint8_t& outPolarity, string& devAddr, string& driver,
                     string& bus) :
        presencePin(presPin),
        presencePolarity(presPolarity), outputPin(outPin),
        outputPolarity(outPolarity), devNameAddr(devAddr), driverType(driver),
        busType(bus)
    {
        eventHandler();
    };

    /*Preserve the previous GPIO value*/
    bool prevPresenceState;

  private:
    /*op-panel GPIO informations to get initialised from vpd json*/
    string presencePin;
    uint8_t presencePolarity;
    string outputPin;
    uint8_t outputPolarity;

    string devNameAddr;
    string driverType;
    string busType;

    /* @brief This API will toggle the gpio as per the presence state.
     */
    void toggleOppanelGpio();

    /* @brief This API checks for an event occurred on GPIO presence pin
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

    /*@brief This API keeps checking for if an event happened
     *       if event occured then take care of action
     */
    void eventHandler();
};

} // namespace gpioMonitor
} // namespace vpd
} // namespace openpower
