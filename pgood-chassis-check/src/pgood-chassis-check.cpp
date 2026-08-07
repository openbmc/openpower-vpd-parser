/**
 * pgood-chassis-check
 *
 * Reads the power-good GPIO for the local chassis and publishes the chassis
 * power state on D-Bus via a set-property call on the Inventory Manager.
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
#include <string>
#include <variant>

namespace pgood_chassis_check
{

/**
 * @brief Read BMC position from /run/openbmc/bmc_position.
 *
 * @return BMC position value, or 0 on any error (file missing, unreadable,
 *         or parse failure). All exceptions are caught locally.
 */
int readBmcPosition() noexcept
{
    /** @todo Read the BMC position integer from bmcPositionFile.
     *  Return 0 as default when the file is absent, unreadable, or fails
     *  to parse. Log a warning via lg2 for each failure mode. */
    return 0;
}

/**
 * @brief Find a GPIO line by name across all chips.
 *
 * @param[in] i_gpioName Name of the GPIO line to find.
 * @return gpiod::line object for the found line, or an empty object if the
 *         line was not found or an exception occurred. All exceptions are
 *         caught locally.
 */
inline gpiod::line findGpioLine(
    [[maybe_unused]] const std::string& i_gpioName) noexcept
{
    /** @todo Call gpiod::find_line(i_gpioName) to locate the named GPIO line
     *  across all chips. Catch any exception, log the error via lg2, and
     *  return an empty gpiod::line{} so the caller treats the chassis as
     *  off. */
    return gpiod::line{};
}

/**
 * @brief Request a GPIO line for input.
 *
 * @param[in] i_line     GPIO line object to request.
 * @param[in] i_gpioName Name of the GPIO line (used for logging only).
 * @return true on success, false on failure. All exceptions are caught
 *         locally.
 */
inline bool requestGpioLine(
    [[maybe_unused]] const gpiod::line& i_line,
    [[maybe_unused]] const std::string& i_gpioName) noexcept
{
    /** @todo Call i_line.request() with DIRECTION_INPUT and consumerName.
     *  Catch any exception, log the error via lg2, and return false so the
     *  caller defaults to chassis off. */
    return false;
}

/**
 * @brief Read the value of a GPIO line.
 *
 * @param[in] i_line     GPIO line object to read from.
 * @param[in] i_gpioName Name of the GPIO line (used for logging only).
 * @return 0 or 1 on success, -1 on failure. All exceptions are caught
 *         locally.
 */
inline int readGpioValue(
    [[maybe_unused]] const gpiod::line& i_line,
    [[maybe_unused]] const std::string& i_gpioName) noexcept
{
    /** @todo Call i_line.get_value() to read the GPIO pin level (0 or 1).
     *  Catch any exception, log the error via lg2, and return -1 so the
     *  caller defaults to chassis off. */
    return -1;
}

/**
 * @brief Set the chassis PowerState on D-Bus via
 * org.freedesktop.DBus.Properties.Set.
 *
 * Calls Set on xyz.openbmc_project.Inventory.Manager at
 * /xyz/openbmc_project/inventory/system to update the
 * xyz.openbmc_project.State.Decorator.PowerState PowerState property.
 *
 * @param[in] i_state PowerState enum value to set.
 * @return true on success, false on failure. All exceptions are caught
 *         locally.
 */
bool setChassisPowerState(
    [[maybe_unused]] PowerStateIface::State i_state) noexcept
{
    /** @todo Convert i_state to its D-Bus string via
     *  PowerStateIface::convertStateToString, then call
     *  org.freedesktop.DBus.Properties.Set on pimServiceName / systemInvPath
     *  for interface PowerStateIface::interface and property
     *  PowerStateIface::property_names::power_state.
     *  Log success via lg2::info and any failure via lg2::error.
     *  Return true on success, false on any exception. */
    return false;
}
}; // namespace pgood_chassis_check

int main()
{
    /** @todo Read the BMC position via readBmcPosition() to select the
     *  correct GPIO line name (gpioLineBmc0 or gpioLineBmc1). Find the line
     *  via findGpioLine(), request it for input via requestGpioLine(), and
     *  read its value via readGpioValue(). Based on the GPIO value (0 = off,
     *  1 = on, -1 = error), call setChassisPowerState() with State::On or
     *  State::Off (default to Off on error). Return 0 on success, 1 if
     *  setChassisPowerState() fails. */
    return 0;
}
