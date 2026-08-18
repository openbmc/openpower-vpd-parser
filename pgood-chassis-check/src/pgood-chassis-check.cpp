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
 * @brief Create an informational PEL via the phosphor-logging D-Bus Create
 * method.
 *
 * @param[in] i_message - The Message property of the event log entry, used to
 * look up the event in the message registry.
 * @param[in] i_description - Human-readable description stored in the PEL's
 * additional data under the DESCRIPTION key.
 */
void createInformationalPel(const std::string& i_message,
                            const std::string& i_description) noexcept
{
    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(
            types::CreateIface::default_service,
            types::CreateIface::instance_path, types::CreateIface::interface,
            types::CreateIface::method_names::create);
        l_method.append(
            i_message, types::EntryIface::Level::Informational,
            std::map<std::string, std::string>{{"DESCRIPTION", i_description}});
        l_bus.call_noreply(l_method);
    }
    catch (const std::exception& l_ex)
    {
        lg2::error(
            "pgood-chassis-check: failed to create informational PEL: {ERR}",
            "ERR", l_ex.what());
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
int publishChassisPowerState(const types::PowerStateIface::State state) noexcept
{
    try
    {
        const std::string stateStr =
            types::PowerStateIface::convertStateToString(state);

        // PIM Notify expects paths relative to the PIM root
        // (/xyz/openbmc_project/inventory), so strip the prefix and pass
        // "/system" as the object path key.

        pgood_chassis_check::types::ObjectMap objectMap;
        objectMap[sdbusplus::object_path{"/system"}]
                 [types::PowerStateIface::interface]
                 [types::PowerStateIface::property_names::power_state] =
                     stateStr;

        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call(constants::pimServiceName, constants::pimPath,
                                constants::pimInterface, "Notify");
        method.append(std::move(objectMap));
        bus.call(method);

        lg2::info(
            "pgood-chassis-check: published PowerState='{STATE}' on D-Bus",
            "STATE", stateStr);
        return constants::success;
    }
    catch (const std::exception& ex)
    {
        lg2::error("pgood-chassis-check: failed to publish PowerState on "
                   "D-Bus: {ERR}",
                   "ERR", ex.what());
        return constants::failure;
    }
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
    try
    {
        types::BmcPosition retVal{types::BmcPosition::INVALID_VALUE};

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
        result.read(bmcPosition);

        lg2::info(
            "pgood-chassis-check: BMC position read from D-Bus is '{VALUE}'",
            "VALUE", bmcPosition);

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
                    "pgood-chassis-check: invalid BMC position value '{VALUE}' read from D-Bus. Returning BMC position as invalid value '{INVALID_VALUE}'",
                    "VALUE", bmcPosition, "INVALID_VALUE",
                    types::BmcPosition::INVALID_VALUE);
            }
        }

        return retVal;
    }
    catch (const std::exception& ex)
    {
        lg2::error(
            "pgood-chassis-check: exception while trying to read BMC position from D-Bus."
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
inline types::GpioValue readGpioValue(const std::string& gpioName) noexcept
{
    try
    {
        gpiod::line line = gpiod::find_line(gpioName);
        if (!line)
        {
            lg2::error("pgood-chassis-check: GPIO line '{GPIO}' not found, "
                       "defaulting to chassis off",
                       "GPIO", gpioName);
            return types::GpioValue::OFF;
        }

        line.request(
            {constants::consumerName, gpiod::line_request::DIRECTION_INPUT, 0});

        const auto value = line.get_value();
        line.release();

        return (value == 0 ? types::GpioValue::OFF : types::GpioValue::ON);
    }
    catch (const std::exception& ex)
    {
        lg2::error("pgood-chassis-check: failed to read GPIO '{GPIO}': "
                   "{ERR}, treating chassis as off",
                   "GPIO", gpioName, "ERR", ex.what());
        return types::GpioValue::INVALID_VALUE;
    }
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
            pgood_chassis_check::createInformationalPel(
                pgood_chassis_check::types::DbusFailureError::errName,
                "pgood-chassis-check: invalid BMC position value read "
                "from D-Bus. Updating chassis power state as off.");

            // could not read the BMC position, so cannot determine which GPIO
            // to read, assume chassis is powered off
            return pgood_chassis_check::publishChassisPowerState(
                pgood_chassis_check::types::PowerStateIface::State::Off);
        }

        // determine which pgood GPIO to read
        const std::string gpioName =
            (bmcPosition == pgood_chassis_check::types::BmcPosition::POSITION_1)
                ? pgood_chassis_check::constants::gpioLineBmc1
                : pgood_chassis_check::constants::gpioLineBmc0;

        lg2::info(
            "pgood-chassis-check: BMC position={POS}, reading GPIO '{GPIO}'",
            "POS", bmcPosition, "GPIO", gpioName);

        // read the GPIO value
        const auto pgoodGpioValue =
            pgood_chassis_check::readGpioValue(gpioName);

        if (pgoodGpioValue == pgood_chassis_check::types::GpioValue::ON)
        {
            lg2::notice(
                "pgood-chassis-check: GPIO '{GPIO}' is 1 - chassis is powered on",
                "GPIO", gpioName);

            return pgood_chassis_check::publishChassisPowerState(
                pgood_chassis_check::types::PowerStateIface::State::On);
        }
        else if (pgoodGpioValue == pgood_chassis_check::types::GpioValue::OFF)
        {
            lg2::info(
                "pgood-chassis-check: GPIO '{GPIO}' is 0 - chassis is powered off",
                "GPIO", gpioName);
        }

        return pgood_chassis_check::publishChassisPowerState(
            pgood_chassis_check::types::PowerStateIface::State::Off);
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
