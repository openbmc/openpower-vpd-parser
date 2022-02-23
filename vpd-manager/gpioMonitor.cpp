#include "gpioMonitor.hpp"

#include "common_utility.hpp"
#include "ibm_vpd_utils.hpp"

#include <systemd/sd-event.h>

#include <chrono>
#include <gpiod.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/utility/timer.hpp>

using namespace std;
using namespace openpower::vpd::constants;
using sdeventplus::ClockId;
using sdeventplus::Event;
using Timer = sdeventplus::utility::Timer<ClockId::Monotonic>;
using namespace std::chrono_literals;

namespace openpower
{
namespace vpd
{
namespace manager
{

bool GpioEventHandler::getPresencePinValue()
{
    Byte gpioData = 1;
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
    string devNameAddr{}, driverType{}, busType{}, objectPath{};

    for (const auto& eachFRU : jsonFile["frus"].items())
    {
        for (const auto& eachInventory : eachFRU.value())
        {
            objectPath = eachInventory["inventoryPath"];

            if ((eachInventory.find("presence") != eachInventory.end()) &&
                (eachInventory.find("preAction") != eachInventory.end()))
            {
                if (!eachInventory["presence"].value("pollingRequired", false))
                {
                    // Polling not required for this FRU , skip.
                    continue;
                }

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

                if ((eachInventory.find("devAddress") != eachInventory.end()) &&
                    (eachInventory.find("driverType") != eachInventory.end()) &&
                    (eachInventory.find("busType") != eachInventory.end()))
                {
                    devNameAddr = eachInventory["devAddress"];
                    driverType = eachInventory["driverType"];
                    busType = eachInventory["busType"];

                    // Init all Gpio info variables
                    std::shared_ptr<GpioEventHandler> gpioObj =
                        make_shared<GpioEventHandler>(
                            presencePinName, presenceValue, outputPinName,
                            outputValue, devNameAddr, driverType, busType,
                            objectPath, event);

                    gpioObjects.push_back(gpioObj);
                }
            }
        }
    }
}

void GpioEventHandler::toggleGpio()
{
    bool presPinVal = getPresencePinValue();
    bool isPresent = false;

    // preserve the new value
    prevPresPinValue = presPinVal;

    if (presPinVal == presenceValue)
    {
        isPresent = true;
    }

    // if FRU went away set the present property to false
    if (!isPresent)
    {
        inventory::ObjectMap objects;
        inventory::InterfaceMap interfaces;
        inventory::PropertyMap presProp;

        presProp.emplace("Present", false);
        interfaces.emplace("xyz.openbmc_project.Inventory.Item", presProp);
        objects.emplace(objectPath, move(interfaces));

        common::utility::callPIM(move(objects));
    }

    gpiod::line outputLine = gpiod::find_line(outputPin);
    if (!outputLine)
    {
        cerr << "Error: toggleGpio: couldn't find output line:" << outputPin
             << ". Skipping update\n";

        return;
    }

    outputLine.request({"FRU presence: update the output GPIO pin",
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
        std::chrono::seconds{5s});

    timers.push_back(timer);
}

} // namespace manager
} // namespace vpd
} // namespace openpower