#include "config.h"

#include "gpioMonitor.hpp"

#include "utils.hpp"

using namespace std;
using namespace openpower::vpd::gpioMonitor;

namespace openpower
{
namespace vpd
{
namespace gpioMonitor
{

bool GpioEventHandler::getCurrentPresenceState()
{
    uint8_t gpioData = 0;
    gpiod::line presenceLine = gpiod::find_line(presencePin);
    if (!presenceLine)
    {
        cout << "Error getCurrentPresenceState: couldn't find presence line:"
             << presencePin << " on GPIO \n";
        exit(-1);
    }

    presenceLine.request(
        {"Op-panel presence line", gpiod::line_request::DIRECTION_INPUT, 0});

    gpioData = presenceLine.get_value();
    if (!(gpioData ^ presencePolarity))
    {
        // it's present
        return true;
    }
    return false;
}

void GpioMonitor::initOppanelGpioInfos()
{

    uint8_t setOrReset = 0;
    uint8_t polarity = 0;
    string presencePinName, outputPinName;
    gpiod::line presenceLine;

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
                    else if (presStatus.key() == "polarity")
                    {
                        if (presStatus.value() == "ACTIVE_HIGH")
                        {
                            polarity = 1;
                        }
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
                GpioEventHandler eventHndlr(presencePinName, polarity,
                                            outputPinName, setOrReset,
                                            devNameAddr, driverType, busType);
            }
        }
    }
}

void GpioEventHandler::toggleOppanelGpio()
{
    auto isPresent = getCurrentPresenceState();
    bool newValue = false;

    if (!(isPresent ^ outputPolarity))
    {
        // set the gpio pin
        newValue = true;
    }

    gpiod::line outputLine = gpiod::find_line(outputPin);
    if (!outputLine)
    {
        cout << "Error: toggleOppanelGpio: couldn't find output line:"
             << outputPin << "\n";

        exit(-1);
    }

    outputLine.request({"FRU present: set the output pin",
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

bool GpioEventHandler::eventOccurred()
{
    bool currentState = getCurrentPresenceState();

    if (currentState == prevPresenceState)
        return false;
    else
        return true;
}

void GpioEventHandler::eventHandler()
{
    prevPresenceState = getCurrentPresenceState();

    unsigned interval = 5;

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

    event.loop();
}

} // namespace gpioMonitor
} // namespace vpd
} // namespace openpower
