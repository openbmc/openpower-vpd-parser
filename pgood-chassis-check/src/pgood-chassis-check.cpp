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
#include <sdbusplus/message/native_types.hpp>

#include <chrono>
#include <expected>
#include <map>
#include <string>
#include <thread>
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
            bus.new_method_call(constants::pimService, constants::pimPath,
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

        try
        {
            line.release();
        }
        catch (const std::exception& ex)
        {
            lg2::error(
                "pgood-chassis-check: error while releasing GPIO '{GPIO}' : "
                "{ERR}",
                "GPIO", gpioName, "ERR", ex.what());
        }

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

/**
 * @brief Method to read chassis power state
 *
 * This method reads the BMC position from D-Bus, decides which pgood GPIO pin
 * to read, reads the selected pgood GPIO. In case this API fails to read the
 * BMC position from D-Bus, it assumes chassis power is off.
 *
 * @return Chassis power state (On/Off) on success; std::unexpected holding
        constants::failure if an exception occurred.
 */
std::expected<types::PowerStateIface::State, int>
    readChassisPowerState() noexcept
{
    try
    {
        // read the BMC position
        const auto bmcPosition = readBmcPositionFromDbus();
        if (bmcPosition == types::BmcPosition::INVALID_VALUE)
        {
            createPel(types::DbusFailureError::errName,
                      "pgood-chassis-check: invalid BMC position value read "
                      "from D-Bus. Updating chassis power state as off.",
                      types::EntryIface::Level::Informational);

            // could not read the BMC position, so cannot determine which GPIO
            // to read, assume chassis is powered off
            return types::PowerStateIface::State::Off;
        }

        // determine which pgood GPIO to read
        const std::string gpioName =
            (bmcPosition == types::BmcPosition::POSITION_1)
                ? constants::gpioLineBmc1
                : constants::gpioLineBmc0;

        lg2::info(
            "pgood-chassis-check: BMC position={POS}, reading GPIO '{GPIO}'",
            "POS", bmcPosition, "GPIO", gpioName);

        // read the GPIO value
        const auto pgoodGpioValue = readGpioValue(gpioName);

        switch (pgoodGpioValue)
        {
            case types::GpioValue::ON:
                lg2::notice(
                    "pgood-chassis-check: GPIO '{GPIO}' is 1 - chassis is powered on",
                    "GPIO", gpioName);
                return types::PowerStateIface::State::On;

            case types::GpioValue::OFF:
                lg2::info(
                    "pgood-chassis-check: GPIO '{GPIO}' is 0 - chassis is powered off",
                    "GPIO", gpioName);
                return types::PowerStateIface::State::Off;

            case types::GpioValue::INVALID_VALUE:
            default:
                lg2::error(
                    "pgood-chassis-check: GPIO '{GPIO}' returned invalid value, "
                    "defaulting to chassis off",
                    "GPIO", gpioName);
                return types::PowerStateIface::State::Off;
        }
    }
    catch (const std::exception& ex)
    {
        lg2::error(
            "pgood-chassis-check: exception while reading the power state: {ERR}.",
            "ERR", ex.what());
        return std::unexpected(constants::failure);
    }
}

/**
 * @brief Request systemd to start a service unit.
 *
 * Issues a D-Bus StartUnit call to org.freedesktop.systemd1.Manager. Uses
 * call() so that any D-Bus-level error surfaces immediately as an exception.
 *
 * @param[in] serviceName - Systemd unit name (e.g. "set-spi-mux.service").
 * @return On success, returns 0, otherwise returns 1.
 */
inline int startService(const std::string& serviceName) noexcept
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            constants::systemdService, constants::systemdObjectPath,
            constants::systemdManagerInterface, "StartUnit");
        method.append(serviceName, "replace");
        bus.call(method);
        return constants::success;
    }
    catch (const std::exception& ex)
    {
        lg2::error(
            "pgood-chassis-check: exception while requesting start of '{SERVICE}': {ERR}.",
            "SERVICE", serviceName, "ERR", ex.what());
        return constants::failure;
    }
}

/**
 * @brief Poll a service unit's ActiveState until it leaves "activating".
 *
 * Resolves the unit's D-Bus object path via GetUnit, then reads the
 * ActiveState and SubState properties every @p pollInterval until the unit
 * reaches a terminal state, or @p maxRetries is exhausted.
 *
 * Terminal states:
 *   - ActiveState="active"              : success (long-running or oneshot
 *                                         with RemainAfterExit=yes)
 *   - ActiveState="inactive",
 *     SubState="dead"                   : success (Type=oneshot completed
 *                                         successfully)
 *   - ActiveState="failed"              : failure
 *
 * Note: GetAll is not used to fetch both properties in one round-trip because
 * org.freedesktop.systemd1.Unit's GetAll returns a{sv} with mixed D-Bus value
 * types that cannot be safely deserialised into a single-type variant map.
 * Two individual Get calls each return a clean v{s}.
 *
 * @param[in] serviceName  - Systemd unit name (e.g. "set-spi-mux.service").
 * @param[in] maxRetries   - Maximum number of polling attempts.
 * @param[in] pollInterval - Time to wait between each polling attempt.
 * @return On success, returns 0, otherwise returns 1.
 */
inline int checkServiceActivation(
    const std::string& serviceName, int maxRetries,
    const std::chrono::milliseconds pollInterval) noexcept
{
    try
    {
        auto bus = sdbusplus::bus::new_default();

        // Resolve the unit's D-Bus object path via GetUnit.
        auto getUnitMethod = bus.new_method_call(
            constants::systemdService, constants::systemdObjectPath,
            constants::systemdManagerInterface, "GetUnit");
        getUnitMethod.append(serviceName);
        auto getUnitReply = bus.call(getUnitMethod);
        sdbusplus::message::object_path unitPath;
        getUnitReply.read(unitPath);

        // Poll ActiveState + SubState until the service finishes starting.
        // Type=oneshot services never linger in "active"; on success they
        // settle to ActiveState="inactive" / SubState="dead". They only reach
        // ActiveState="failed" on an actual failure.
        for (int i = 0; i < maxRetries; ++i)
        {
            // Read ActiveState and SubState via separate Get calls.
            // GetAll returns a{sv} with mixed value types and cannot be
            // safely read into a single-type variant map.
            auto getActive = bus.new_method_call(
                constants::systemdService, unitPath.str.c_str(),
                "org.freedesktop.DBus.Properties", "Get");
            getActive.append("org.freedesktop.systemd1.Unit", "ActiveState");
            std::variant<std::string> activeStateVariant;
            bus.call(getActive).read(activeStateVariant);
            const auto& activeState = std::get<std::string>(activeStateVariant);

            auto getSub = bus.new_method_call(
                constants::systemdService, unitPath.str.c_str(),
                "org.freedesktop.DBus.Properties", "Get");
            getSub.append("org.freedesktop.systemd1.Unit", "SubState");
            std::variant<std::string> subStateVariant;
            bus.call(getSub).read(subStateVariant);
            const auto& subState = std::get<std::string>(subStateVariant);

            // "active" covers long-running services or oneshot with
            // RemainAfterExit=yes.
            if (activeState == "active")
            {
                lg2::info(
                    "pgood-chassis-check: '{SERVICE}' started successfully",
                    "SERVICE", serviceName);
                return constants::success;
            }

            // Type=oneshot success: process exited 0, unit winds down to
            // inactive/dead.
            if (activeState == "inactive" && subState == "dead")
            {
                lg2::info(
                    "pgood-chassis-check: '{SERVICE}' completed successfully (oneshot)",
                    "SERVICE", serviceName);
                return constants::success;
            }

            if (activeState == "failed")
            {
                lg2::error(
                    "pgood-chassis-check: '{SERVICE}' failed (ActiveState='{STATE}', SubState='{SUBSTATE}')",
                    "SERVICE", serviceName, "STATE", activeState, "SUBSTATE",
                    subState);
                return constants::failure;
            }

            // Still "activating" or "deactivating" — wait and retry.
            std::this_thread::sleep_for(pollInterval);
        }

        lg2::error(
            "pgood-chassis-check: timed out waiting for '{SERVICE}' to start",
            "SERVICE", serviceName);
        return constants::failure;
    }
    catch (const std::exception& ex)
    {
        lg2::error(
            "pgood-chassis-check: exception while checking activation of '{SERVICE}': {ERR}.",
            "SERVICE", serviceName, "ERR", ex.what());
        return constants::failure;
    }
}

} // namespace pgood_chassis_check

int main()
{
    try
    {
        // assume power state as off if we failed to read the chassis power
        // state
        const auto chassisPowerState =
            pgood_chassis_check::readChassisPowerState().value_or(
                pgood_chassis_check::types::PowerStateIface::State::Off);

        if (chassisPowerStateValue ==
            pgood_chassis_check::types::PowerStateIface::State::Off)
        {
            static constexpr int maxRetries = 20;
            static constexpr auto pollInterval = std::chrono::milliseconds(500);

            // Start the service, then verify it reached "active".
            if (pgood_chassis_check::constants::failure ==
                pgood_chassis_check::startService(
                    pgood_chassis_check::constants::setSpiMuxServiceName))
            {
                lg2::error(
                    "pgood-chassis-check: failed to request start of set-spi-mux service");
            }
            else if (pgood_chassis_check::constants::failure ==
                     pgood_chassis_check::checkServiceActivation(
                         pgood_chassis_check::constants::setSpiMuxServiceName,
                         maxRetries, pollInterval))
            {
                lg2::error(
                    "pgood-chassis-check: set-spi-mux service did not activate successfully");
            }
        }
        else
        {
            lg2::info(
                "pgood-chassis-check: chassis powered on, not triggering set-spi-mux service");
        }

        // publish the chassis power state
        return pgood_chassis_check::publishChassisPowerState(chassisPowerState);
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
