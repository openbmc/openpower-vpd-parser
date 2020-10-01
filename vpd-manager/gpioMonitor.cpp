#include "gpioMonitor.hpp"

#include "ibm_vpd_utils.hpp"

#include <systemd/sd-event.h>

#include <gpiod.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

using namespace std;
using namespace openpower::vpd::constants;
using sdeventplus::ClockId;
using sdeventplus::Event;
constexpr auto clockId = ClockId::RealTime;
using Timer = sdeventplus::utility::Timer<clockId>;

namespace openpower
{
namespace vpd
{
namespace manager
{
namespace gpiomonitor
{

bool GpioEventHandler::getPresencePinValue()
{
    Byte gpioData = 0;
    gpiod::line presenceLine = gpiod::find_line(presencePin);
    if (!presenceLine)
    {
        cerr << "Error getPresencePinValue: couldn't find presence line:"
             << presencePin << " on GPIO \n";
        // return previous state as we couldn't read current state
        return prevPresPinValue;
    }

    presenceLine.request(
        {"Op-panel presence line", gpiod::line_request::DIRECTION_INPUT, 0});

    gpioData = presenceLine.get_value();
    if (gpioData)
    {
        // pin value is 1
        return true;
    }
    else
    {
        // pin value is 0
        return false;
    }
}

void GpioMonitor::initGpioInfos(std::shared_ptr<GpioEventHandler>& gpioObj,
                                const std::string& requestedGpioPin,
                                std::shared_ptr<Timer>& timer, Event& event)
{
    Byte outputValue = 0;
    Byte PresenceValue = 0;
    string presencePinName{}, outputPinName{};
    string devNameAddr{}, driverType{}, busType{};

    for (const auto& eachFRU : jsonFile["frus"].items())
    {
        for (const auto& eachInventory : eachFRU.value())
        {
            if ((eachInventory.find("presence") != eachInventory.end()) &&
                (eachInventory.find("preAction") != eachInventory.end()))
            {
                for (const auto& presStatus : eachInventory["presence"].items())
                {
                    if (presStatus.key() == "pin")
                    {
                        presencePinName = presStatus.value();

                        // check if it desired FRU's information
                        if (requestedGpioPin != presencePinName)
                        {
                            break;
                        }
                    }
                    else if (presStatus.key() == "value")
                    {
                        PresenceValue = presStatus.value();
                    }
                }

                if (requestedGpioPin != presencePinName)
                {
                    continue;
                }
                // Based on presence pin value, preAction pin will be set/reset
                // This action will be taken before vpd collection, so named as
                // preAction.
                for (const auto& preAction : eachInventory["preAction"].items())
                {
                    if (preAction.key() == "pin")
                    {
                        outputPinName = preAction.value();
                    }
                    else if (preAction.key() == "value")
                    {
                        outputValue = preAction.value();
                    }
                }

                devNameAddr = eachInventory["devAddress"];
                driverType = eachInventory["driverType"];
                busType = eachInventory["busType"];

                // Init all Gpio info variables
                // GpioEventHandler eventHndlr(
                gpioObj = make_shared<GpioEventHandler>(
                    presencePinName, PresenceValue, outputPinName, outputValue,
                    devNameAddr, driverType, busType, timer, event);

                // Stored all the datas, now No need to iterate to next
                // Inventory
                break;
            }
        }
        // gpioObj initialised with all the datas, now No need to iterate to
        // next FRU
        if (gpioObj != nullptr)
        {
            break;
        }
    }
}

bool GpioEventHandler::hasEventOccurred()
{
    return getPresencePinValue() != prevPresPinValue;
}

void GpioEventHandler::toggleGpio()
{
    bool presPinVal = getPresencePinValue();
    bool isPresent = false;

    if (!(presPinVal ^ presenceValue))
    {
        // set the gpio pin
        isPresent = true;
    }

    gpiod::line outputLine = gpiod::find_line(outputPin);
    if (!outputLine)
    {
        cerr << "Error: toggleGpio: couldn't find output line:" << outputPin
             << ". Skipping update\n";

        return;
    }

    outputLine.request({"Op-panel LCD present: set the output pin",
                        gpiod::line_request::DIRECTION_OUTPUT, 0},
                       isPresent ? outputValue : (!outputValue));

    string driverCmnd = createBindUnbindDriverCmnd(
        devNameAddr, busType, driverType, isPresent ? "/bind" : "/unbind");

    int ret = system(driverCmnd.c_str());
    if (ret == -1)
    {
        cerr << "System call Failed to run the command of binding/Unbinding\n";
    }

    prevPresPinValue = presPinVal;
}

void GpioEventHandler::doEventAndTimerSetup(shared_ptr<Timer>& timer,
                                            sdeventplus::Event& event)
{
    prevPresPinValue = getPresencePinValue();

    unsigned interval = FIVE_SEC;

    timer = make_shared<Timer>(
        event,
        [this](Timer&) {
            if (hasEventOccurred())
            {
                toggleGpio();
            }
        },
        std::chrono::seconds{interval});
}

} // namespace gpiomonitor
} // namespace manager
} // namespace vpd
} // namespace openpower