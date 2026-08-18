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
 * @return 0 on success, 1 on failure. All exceptions are caught
 *         locally.
 */
int publishChassisPowerState(
    [[maybe_unused]] const types::PowerStateIface::State state) noexcept
{
    /** @todo Convert state to its D-Bus string via
     *  PowerStateIface::convertStateToString, then call PIM Notify on
     *  pimServiceName / pimPath with a relative object path ("/system"),
     *  interface PowerStateIface::interface, and property
     *  PowerStateIface::property_names::power_state set to the state string.
     *  Log success via lg2::info and any failure via lg2::error.
     *  Return true on success, false on any exception. */
    return constants::success;
}

/**
 * @brief Read BMC position from file /run/openbmc/bmc_position.
 *
 * @return BMC position value, or -1 on any error (file missing, unreadable,
 *         or parse failure). All exceptions are caught locally.
 */
types::BmcPosition readBmcPositionFromFile() noexcept
{
    try
    {
        std::error_code ec{};
        if (!std::filesystem::exists(constants::bmcPositionFile, ec))
        {
            if (ec)
            {
                lg2::warning(
                    "pgood-chassis-check: could not stat '{FILE}': "
                    "{ERR}, Returning BMC position as invalid value '{INVALID_VALUE}'",
                    "FILE", constants::bmcPositionFile, "ERR", ec.message(),
                    "INVALID_VALUE", types::BmcPosition::INVALID_VALUE);
            }
            else
            {
                lg2::info(
                    "pgood-chassis-check: '{FILE}' not found, "
                    "Returning BMC position as invalid value '{INVALID_VALUE}'",
                    "FILE", constants::bmcPositionFile, "INVALID_VALUE",
                    types::BmcPosition::INVALID_VALUE);
            }
            return types::BmcPosition::INVALID_VALUE;
        }

        std::ifstream ifs(constants::bmcPositionFile);
        if (!ifs)
        {
            lg2::warning(
                "pgood-chassis-check: failed to open '{FILE}', "
                "Returning BMC position as invalid value '{INVALID_VALUE}'",
                "FILE", constants::bmcPositionFile, "INVALID_VALUE",
                types::BmcPosition::INVALID_VALUE);

            return types::BmcPosition::INVALID_VALUE;
        }

        int bmcPosition{std::to_underlying(types::BmcPosition::INVALID_VALUE)};
        if (!(ifs >> bmcPosition))
        {
            lg2::warning(
                "pgood-chassis-check: failed to parse '{FILE}', "
                "Returning BMC position as invalid value '{INVALID_VALUE}'",
                "FILE", constants::bmcPositionFile, "INVALID_VALUE",
                types::BmcPosition::INVALID_VALUE);

            return types::BmcPosition::INVALID_VALUE;
        }

        ifs.close();

        types::BmcPosition retVal{types::BmcPosition::INVALID_VALUE};

        switch (bmcPosition)
        {
            case 0:
            {
                retVal = types::BmcPosition::POSITION_0;
                break;
            }
            case 1:
            {
                retVal = types::BmcPosition::POSITION_1;
                break;
            }
            default:
            {
                retVal = types::BmcPosition::INVALID_VALUE;
                lg2::error(
                    "pgood-chassis-check: invalid BMC position value '{VALUE}' read from file. Returning BMC position as invalid value '{INVALID_VALUE}'",
                    "VALUE", bmcPosition, "INVALID_VALUE",
                    types::BmcPosition::INVALID_VALUE);
            }
        }

        return retVal;
    }
    catch (const std::exception& ex)
    {
        lg2::error(
            "pgood-chassis-check: exception while trying to read BMC position from file."
            "{ERR}. Returning BMC position as default value, '{INVALID_VALUE}'",
            "ERR", ex.what(), "INVALID_VALUE",
            types::BmcPosition::INVALID_VALUE);
    }
    return types::BmcPosition::INVALID_VALUE;
}

/**
 * @brief Read the value of a GPIO line.
 *
 * @param[in] gpioName Name of the GPIO line
 * @return 0 or 1 on success, -1 on failure. All exceptions are caught
 *         locally.
 */
inline types::GpioValue readGpioValue(
    [[maybe_unused]] const std::string& gpioName) noexcept
{
    /** @todo
     *  1. Call gpiod::find_line(gpioName) to locate the named GPIO line
     *  across all chips. Catch any exception, log the error via lg2, and
     *  return an empty gpiod::line{} so the caller treats the chassis as
     *  off.
     *  2. Call line.request() with DIRECTION_INPUT and consumerName.
     *  Catch any exception, log the error via lg2, and return false so the
     *  caller defaults to chassis off.
     *  3. Call line.get_value() to read the GPIO pin level (0 or 1).
     *  Catch any exception, log the error via lg2, and return -1 so the
     *  caller defaults to chassis off. */
    return types::GpioValue::INVALID_VALUE;
}
} // namespace pgood_chassis_check

int main()
{
    try
    {
        // read the BMC position
        const auto bmcPosition = pgood_chassis_check::readBmcPositionFromFile();
        if (bmcPosition ==
            pgood_chassis_check::types::BmcPosition::INVALID_VALUE)
        {
            lg2::error(
                "pgood-chassis-check: invalid BMC position value read from file. Updating chassis power state as off to allow FRUs VPD collection to proceed.");

            // could not read the BMC position, so cannot determine which GPIO
            // to read, assume chassis is powered off to allow FRUs VPD
            // collection to proceed
            return pgood_chassis_check::publishChassisPowerState(
                pgood_chassis_check::types::PowerStateIface::State::Off);
        }

        /** @todo
         * 1. Select GPIO pin based on BMC position and read its value via
         * readGpioValue().
         * 2. Based on the GPIO value (0 = off,
         *  1 = on, -1 = error), call publishChassisPowerState() with State::On
         * or State::Off (default to Off on error).
         * 3. Return 0 on success, 1 if
         *  publishChassisPowerState() fails. */
    }
    catch (const std::exception& ex)
    {
        lg2::error(
            "pgood-chassis-check: exception in main: {ERR}. Returning failure",
            "ERR", ex.what());
        return pgood_chassis_check::constants::failure;
    }
    return pgood_chassis_check::constants::success;
}
