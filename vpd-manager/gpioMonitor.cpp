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
using Timer = sdeventplus::utility::Timer<ClockId::Monotonic>;

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

    return gpioData;
}

void GpioMonitor::initGpioInfos(Event& event)
{
    Byte outputValue = 0;
    Byte presenceValue = 0;
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
                    }
                    else if (presStatus.key() == "value")
                    {
                        presenceValue = presStatus.value();
                    }
                }

                // Based on presence pin value, preAction pin will be set/reset
                // This action will be taken before vpd collection.
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
                std::shared_ptr<GpioEventHandler> gpioObj =
                    make_shared<GpioEventHandler>(
                        presencePinName, presenceValue, outputPinName,
                        outputValue, devNameAddr, driverType, busType, event);

                gpioObjects.push_back(gpioObj);
            }
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

    // preserve the new value
    prevPresPinValue = presPinVal;

    if (presPinVal == presenceValue)
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

    string cmnd = createBindUnbindDriverCmnd(devNameAddr, busType, driverType,
                                             isPresent ? "bind" : "unbind");

    cout << cmnd << endl;
    executeCmd(cmnd);
}

void GpioEventHandler::doEventAndTimerSetup(sdeventplus::Event& event)
{
    prevPresPinValue = getPresencePinValue();

    static vector<shared_ptr<Timer>> timers;
    shared_ptr<Timer> timer = make_shared<Timer>(
        event,
        [this](Timer&) {
            if (hasEventOccurred())
            {
                toggleGpio();
            }
        },
        std::chrono::seconds{FIVE_SEC});

    timers.push_back(timer);
}

} // namespace gpiomonitor
} // namespace manager
} // namespace vpd
} // namespace openpower