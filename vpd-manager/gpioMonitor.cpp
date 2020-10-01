#include "config.h"

#include "gpioMonitor.hpp"

#include "utils.hpp"

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
namespace gpioMonitor
{

bool GpioEventHandler::getCurrentPresenceState()
{
    Byte gpioData = 0;
    gpiod::line presenceLine = gpiod::find_line(presencePin);
    if (!presenceLine)
    {
        cout << "Error getCurrentPresenceState: couldn't find presence line:"
             << presencePin << " on GPIO \n";
        // return previous state as we couldn't read current state
        return prevPresenceState;
    }

    presenceLine.request(
        {"Op-panel presence line", gpiod::line_request::DIRECTION_INPUT, 0});

    gpioData = presenceLine.get_value();
    if (!(gpioData ^ presenceValue))
    {
        // it's present
        return true;
    }
    return false;
}

void GpioMonitor::initOppanelGpioInfos()
{
    Byte setOrReset = 0;
    Byte value = 0;
    string presencePinName{}, outputPinName{};

    for (const auto& eachFRU : jsonFile["frus"].items())
    {
        for (const auto& eachInventory : eachFRU.value())
        {
            if (eachInventory.find("preAction") != eachInventory.end())
            {
                for (const auto& presStatus : eachInventory["presence"].items())
                {
                    if (presStatus.key() == "pin")
                    {
                        presencePinName = presStatus.value();
                    }
                    else if (presStatus.key() == "value")
                    {
                        value = presStatus.value();
                    }
                }

                for (const auto& preAction : eachInventory["preAction"].items())
                {
                    if (preAction.key() == "pin")
                    {
                        outputPinName = preAction.value();
                    }
                    else if (preAction.key() == "value")
                    {
                        setOrReset = preAction.value();
                    }
                }

                string devNameAddr = eachInventory["devAddress"];
                string driverType = eachInventory["driverType"];
                string busType = eachInventory["busType"];

                // Init all Gpio info variable
                GpioEventHandler eventHndlr(
                    presencePinName, value, outputPinName, setOrReset,
                    devNameAddr, driverType, busType, _bus);
            }
        }
    }
}

void GpioEventHandler::toggleOppanelGpio()
{
    bool isPresent = getCurrentPresenceState();
    bool newValue = false;

    if (!(isPresent ^ outputValue))
    {
        // set the gpio pin
        newValue = true;
    }

    gpiod::line outputLine = gpiod::find_line(outputPin);
    if (!outputLine)
    {
        cout << "Error: toggleOppanelGpio: couldn't find output line:"
             << outputPin << ". Skipping update\n";

        return;
    }

    outputLine.request({"Op-panel LCD present: set the output pin",
                        gpiod::line_request::DIRECTION_OUTPUT, 0},
                       newValue);

    string str = "echo ";
    if (isPresent)
    {
        // bind the driver
        str = str + devNameAddr + " > /sys/bus/" + busType + "/drivers/" +
              driverType + "/bind";
    }
    else
    {
        // unbind the driver
        str = str + devNameAddr + " > /sys/bus/" + busType + "/drivers/" +
              driverType + "/unbind";
    }
    system(str.c_str());

    prevPresenceState = isPresent;
}

inline bool GpioEventHandler::eventOccurred()
{
    return getCurrentPresenceState() != prevPresenceState;
}

void GpioEventHandler::eventHandler()
{
    prevPresenceState = getCurrentPresenceState();
    unsigned interval = FIVE_SEC;

    auto event = Event::get_default();
    Timer timer(
        event,
        [this](Timer&) {
            if (eventOccurred())
            {
                toggleOppanelGpio();
            }
        },
        std::chrono::seconds{interval});

    _dbus.attach_event(event.get(), SD_EVENT_PRIORITY_IMPORTANT);
    event.loop();
}

} // namespace gpioMonitor
} // namespace manager
} // namespace vpd
} // namespace openpower
