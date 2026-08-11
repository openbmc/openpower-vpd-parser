/**
 * pgood-chassis-check
 *
 * Reads the power-good GPIO for the local chassis and publishes the chassis
 * power state on D-Bus via a Notify call on Phosphor Inventory Manager service.
 *
 * BMC position is read from /run/openbmc/bmc_position:
 *   0 (or file missing/unreadable) -> read GPIO "power-good-chassis1"
 *   1                               -> read GPIO "power-good-chassis2"
 *
 * Outcome published on D-Bus at:
 *   service  : xyz.openbmc_project.Inventory.Manager
 *   path     : /xyz/openbmc_project/inventory/system
 *   interface: xyz.openbmc_project.State.Decorator.PowerState
 *   property : PowerState
 *
 *   GPIO=0 (chassis off): PowerState = State::Off
 *   GPIO=1 (chassis on):  PowerState = State::On
 *
 * wait-vpd-parsers.service reads this property via D-Bus to decide whether
 * to run full VPD collection (Off) or just mark collection complete (On).
 */

#include "constants.hpp"
#include "types.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <variant>

namespace pgood_chassis_check
{
/**
 * @brief Publish the chassis PowerState on D-Bus via PIM Notify.
 *
 * Calls xyz.openbmc_project.Inventory.Manager Notify to create/update the
 * xyz.openbmc_project.State.Decorator.PowerState interface and its
 * PowerState property at /xyz/openbmc_project/inventory/system.
 *
 * @param[in] state PowerState enum value to set.
 * @return true on success, false on failure. All exceptions are caught
 *         locally.
 */
bool publishChassisPowerState(PowerStateIface::State state) noexcept
{
    try
    {
        const std::string stateStr =
            PowerStateIface::convertStateToString(state);

        // PIM Notify expects paths relative to the PIM root
        // (/xyz/openbmc_project/inventory), so strip the prefix and pass
        // "/system" as the object path key.

        ObjectMap objectMap;
        objectMap[sdbusplus::object_path{"/system"}][PowerStateIface::interface]
                 [PowerStateIface::property_names::power_state] = stateStr;

        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call(constants::pimServiceName, constants::pimPath,
                                constants::pimInterface, "Notify");
        method.append(std::move(objectMap));
        bus.call(method);

        lg2::info(
            "pgood-chassis-check: published PowerState='{STATE}' on D-Bus",
            "STATE", stateStr);
        return true;
    }
    catch (const std::exception& ex)
    {
        lg2::error("pgood-chassis-check: failed to publish PowerState on "
                   "D-Bus: {ERR}",
                   "ERR", ex.what());
        return false;
    }
}

/**
 * @brief Read BMC position from /run/openbmc/bmc_position.
 *
 * @return BMC position value, or 0 on any error (file missing, unreadable,
 *         or parse failure). All exceptions are caught locally.
 */
int readBmcPosition() noexcept
{
    std::error_code ec{};
    if (!std::filesystem::exists(constants::bmcPositionFile, ec))
    {
        if (ec)
        {
            lg2::warning("pgood-chassis-check: could not stat '{FILE}': "
                         "{ERR}, defaulting BMC position to 0",
                         "FILE", constants::bmcPositionFile, "ERR",
                         ec.message());
        }
        else
        {
            lg2::info("pgood-chassis-check: '{FILE}' not found, "
                      "defaulting BMC position to 0",
                      "FILE", constants::bmcPositionFile);
        }
        return 0;
    }

    std::ifstream ifs(constants::bmcPositionFile);
    if (!ifs)
    {
        lg2::warning("pgood-chassis-check: failed to open '{FILE}', "
                     "defaulting BMC position to 0",
                     "FILE", constants::bmcPositionFile);
        return 0;
    }

    int pos = 0;
    if (!(ifs >> pos))
    {
        lg2::warning("pgood-chassis-check: failed to parse '{FILE}', "
                     "defaulting BMC position to 0",
                     "FILE", constants::bmcPositionFile);
        return 0;
    }

    return pos;
}

/**
 * @brief Read the value of a GPIO line.
 *
 * @param[in] gpioName Name of the GPIO line
 * @return 0 or 1 on success, -1 on failure. All exceptions are caught
 *         locally.
 */
inline int readGpioValue(const std::string& gpioName) noexcept
{
    try
    {
        gpiod::line line = gpiod::find_line(gpioName);
        if (!line)
        {
            lg2::error("pgood-chassis-check: GPIO line '{GPIO}' not found, "
                       "defaulting to chassis off",
                       "GPIO", gpioName);
            return 0;
        }

        line.request(
            {constants::consumerName, gpiod::line_request::DIRECTION_INPUT, 0});

        const auto value = line.get_value();
        line.release();

        return value;
    }
    catch (const std::exception& ex)
    {
        lg2::error("pgood-chassis-check: failed to read GPIO '{GPIO}': "
                   "{ERR}, treating chassis as off",
                   "GPIO", gpioName, "ERR", ex.what());
        return -1;
    }
}
}; // namespace pgood_chassis_check

int main()
{
    // Determine which GPIO line to read based on BMC position
    const int bmcPos = pgood_chassis_check::readBmcPosition();
    const std::string gpioName =
        (bmcPos == 1) ? pgood_chassis_check::constants::gpioLineBmc1
                      : pgood_chassis_check::constants::gpioLineBmc0;

    lg2::info("pgood-chassis-check: BMC position={POS}, reading GPIO '{GPIO}'",
              "POS", bmcPos, "GPIO", gpioName);

    // Read the GPIO value
    const int pgood = pgood_chassis_check::readGpioValue(gpioName);

    if (pgood == -1)
    {
        // Error already logged in readGpioValue; default to chassis off
        return pgood_chassis_check::publishChassisPowerState(
                   pgood_chassis_check::PowerStateIface::State::Off)
                   ? 0
                   : 1;
    }

    if (pgood == 1)
    {
        lg2::notice(
            "pgood-chassis-check: GPIO '{GPIO}' is 1 - chassis is powered on",
            "GPIO", gpioName);
        return pgood_chassis_check::publishChassisPowerState(
                   pgood_chassis_check::PowerStateIface::State::On)
                   ? 0
                   : 1;
    }

    lg2::info(
        "pgood-chassis-check: GPIO '{GPIO}' is 0 - chassis is powered off",
        "GPIO", gpioName);
    return pgood_chassis_check::publishChassisPowerState(
               pgood_chassis_check::PowerStateIface::State::Off)
               ? 0
               : 1;
}
