/**
 * pgood-chassis-check
 *
 * Reads the power-good GPIO for the local chassis and publishes the chassis
 * power state on D-Bus via a Notify call on Phosphor Inventory Manager service.
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

#include <map>
#include <string>
#include <utility>
#include <variant>

namespace pgood_chassis_check
{
/**
 * @brief Create a PEL via the phosphor-logging D-Bus Create method.
 *
 * @param[in] message - The Message property of the event log entry, used to
 * look up the event in the message registry.
 * @param[in] description - Human-readable description stored in the PEL's
 * additional data under the DESCRIPTION key.
 * @param[in] level - Severity level of the event log entry
 * (e.g. Informational, Error, Warning).
 */
void createPel(const std::string& message, const std::string& description,
               const types::EntryIface::Level level) noexcept
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            types::CreateIface::default_service,
            types::CreateIface::instance_path, types::CreateIface::interface,
            types::CreateIface::method_names::create);
        method.append(
            message, level,
            std::map<std::string, std::string>{{"DESCRIPTION", description}});
        bus.call_noreply(method);
    }
    catch (const std::exception& ex)
    {
        lg2::error("pgood-chassis-check: failed to create PEL: {ERR}", "ERR",
                   ex.what());
    }
}

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
 * @brief Read BMC position from D-Bus
 *
 * This method reads the BMC position published on Position interface on
 * /xyz/openbmc_project/inventory/system of phosphor inventory manager and
 * returns the corresponding enum value.
 *
 * @return BMC position value, or -1 on any error. All exceptions are caught
 * locally.
 */
types::BmcPosition readBmcPositionFromDbus() noexcept
{
    types::BmcPosition retVal{types::BmcPosition::INVALID_VALUE};
    try
    {
        size_t bmcPosition{
            static_cast<size_t>(types::BmcPosition::INVALID_VALUE)};

        // read BMC position from D-Bus
        auto bus = sdbusplus::bus::new_default();

        auto method = bus.new_method_call(
            constants::pimService, constants::systemVpdInvPath,
            "org.freedesktop.DBus.Properties", "Get");

        method.append(constants::positionInterface,
                      constants::positionPropertyName);

        auto result = bus.call(method);
        std::variant<size_t> variantPosition;
        result.read(variantPosition);
        bmcPosition = std::get<size_t>(variantPosition);

        lg2::info(
            "pgood-chassis-check: BMC position read from D-Bus is '{VALUE}'",
            "VALUE", bmcPosition);

        if (bmcPosition == std::to_underlying(types::BmcPosition::POSITION_0) ||
            bmcPosition == std::to_underlying(types::BmcPosition::POSITION_1))
        {
            retVal = static_cast<types::BmcPosition>(bmcPosition);
        }
        else
        {
            lg2::error(
                "pgood-chassis-check: invalid BMC position value '{VALUE}' read from D-Bus. Returning BMC position as invalid value '{INVALID_VALUE}'",
                "VALUE", bmcPosition, "INVALID_VALUE",
                types::BmcPosition::INVALID_VALUE);
        }
    }
    catch (const std::exception& ex)
    {
        lg2::error(
            "pgood-chassis-check: exception while trying to read BMC position from D-Bus. "
            "{ERR}. Returning BMC position as default value, '{INVALID_VALUE}'",
            "ERR", ex.what(), "INVALID_VALUE",
            types::BmcPosition::INVALID_VALUE);
    }
    return retVal;
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
        const auto bmcPosition = pgood_chassis_check::readBmcPositionFromDbus();
        if (bmcPosition ==
            pgood_chassis_check::types::BmcPosition::INVALID_VALUE)
        {
            pgood_chassis_check::createPel(
                pgood_chassis_check::types::DbusFailureError::errName,
                "pgood-chassis-check: invalid BMC position value read "
                "from D-Bus. Updating chassis power state as off.",
                pgood_chassis_check::types::EntryIface::Level::Informational);

            // could not read the BMC position, so cannot determine which GPIO
            // to read, assume chassis is powered off
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
