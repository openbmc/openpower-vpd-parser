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
 * @brief Read BMC position from /run/openbmc/bmc_position.
 *
 * @return BMC position value, or 0 on any error (file missing, unreadable,
 *         or parse failure). All exceptions are caught locally.
 */
int readBmcPosition() noexcept
{
    std::error_code l_ec{};
    if (!std::filesystem::exists(bmcPositionFile, l_ec))
    {
        if (l_ec)
        {
            lg2::warning("pgood-chassis-check: could not stat '{FILE}': "
                         "{ERR}, defaulting BMC position to 0",
                         "FILE", bmcPositionFile, "ERR", l_ec.message());
        }
        else
        {
            lg2::info("pgood-chassis-check: '{FILE}' not found, "
                      "defaulting BMC position to 0",
                      "FILE", bmcPositionFile);
        }
        return 0;
    }

    std::ifstream l_ifs(bmcPositionFile);
    if (!l_ifs)
    {
        lg2::warning("pgood-chassis-check: failed to open '{FILE}', "
                     "defaulting BMC position to 0",
                     "FILE", bmcPositionFile);
        return 0;
    }

    int l_pos = 0;
    if (!(l_ifs >> l_pos))
    {
        lg2::warning("pgood-chassis-check: failed to parse '{FILE}', "
                     "defaulting BMC position to 0",
                     "FILE", bmcPositionFile);
        return 0;
    }

    return l_pos;
}

/**
 * @brief Find a GPIO line by name across all chips.
 *
 * @param[in] i_gpioName Name of the GPIO line to find.
 * @return gpiod::line object for the found line, or an empty object if the
 *         line was not found or an exception occurred. All exceptions are
 *         caught locally.
 */
inline gpiod::line findGpioLine(const std::string& i_gpioName) noexcept
{
    try
    {
        return gpiod::find_line(i_gpioName);
    }
    catch (const std::exception& l_ex)
    {
        lg2::error("pgood-chassis-check: exception while searching for GPIO "
                   "line '{GPIO}': {ERR}, treating chassis as off",
                   "GPIO", i_gpioName, "ERR", l_ex.what());
        return gpiod::line{};
    }
}

/**
 * @brief Request a GPIO line for input.
 *
 * @param[in] i_line     GPIO line object to request.
 * @param[in] i_gpioName Name of the GPIO line (used for logging only).
 * @return true on success, false on failure. All exceptions are caught
 *         locally.
 */
inline bool requestGpioLine(const gpiod::line& i_line,
                            const std::string& i_gpioName) noexcept
{
    try
    {
        i_line.request({consumerName, gpiod::line_request::DIRECTION_INPUT, 0});
        return true;
    }
    catch (const std::exception& l_ex)
    {
        lg2::error("pgood-chassis-check: failed to request GPIO '{GPIO}': "
                   "{ERR}, treating chassis as off",
                   "GPIO", i_gpioName, "ERR", l_ex.what());
        return false;
    }
}

/**
 * @brief Read the value of a GPIO line.
 *
 * @param[in] i_line     GPIO line object to read from.
 * @param[in] i_gpioName Name of the GPIO line (used for logging only).
 * @return 0 or 1 on success, -1 on failure. All exceptions are caught
 *         locally.
 */
inline int readGpioValue(const gpiod::line& i_line,
                         const std::string& i_gpioName) noexcept
{
    try
    {
        return i_line.get_value();
    }
    catch (const std::exception& l_ex)
    {
        lg2::error("pgood-chassis-check: failed to read GPIO '{GPIO}': "
                   "{ERR}, treating chassis as off",
                   "GPIO", i_gpioName, "ERR", l_ex.what());
        return -1;
    }
}

/**
 * @brief Publish the chassis PowerState on D-Bus via PIM Notify.
 *
 * Calls xyz.openbmc_project.Inventory.Manager Notify to create/update the
 * xyz.openbmc_project.State.Decorator.PowerState interface and its
 * PowerState property at /xyz/openbmc_project/inventory/system.
 *
 * @param[in] i_state PowerState enum value to publish.
 * @return true on success, false on failure. All exceptions are caught
 *         locally.
 */
bool setChassisPowerState(PowerStateIface::State i_state) noexcept
{
    try
    {
        const std::string l_stateStr =
            PowerStateIface::convertStateToString(i_state);

        // PIM Notify expects paths relative to the PIM root
        // (/xyz/openbmc_project/inventory), so strip the prefix and pass
        // "/system" as the object path key.
        using PropertyMap = std::map<std::string, std::variant<std::string>>;
        using InterfaceMap = std::map<std::string, PropertyMap>;
        using ObjectMap = std::map<sdbusplus::object_path, InterfaceMap>;

        ObjectMap l_objectMap;
        l_objectMap[sdbusplus::object_path{"/system"}]
                   [PowerStateIface::interface]
                   [PowerStateIface::property_names::power_state] = l_stateStr;

        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(pimServiceName, pimPath,
                                              pimInterface, "Notify");
        l_method.append(std::move(l_objectMap));
        l_bus.call(l_method);

        lg2::info(
            "pgood-chassis-check: published PowerState='{STATE}' on D-Bus",
            "STATE", l_stateStr);
        return true;
    }
    catch (const std::exception& l_ex)
    {
        lg2::error("pgood-chassis-check: failed to publish PowerState on "
                   "D-Bus: {ERR}",
                   "ERR", l_ex.what());
        return false;
    }
}
}; // namespace pgood_chassis_check

int main()
{
    // Determine which GPIO line to read based on BMC position
    const int l_bmcPos = pgood_chassis_check::readBmcPosition();
    const std::string l_gpioName =
        (l_bmcPos == 1) ? pgood_chassis_check::gpioLineBmc1
                        : pgood_chassis_check::gpioLineBmc0;

    lg2::info("pgood-chassis-check: BMC position={POS}, reading GPIO '{GPIO}'",
              "POS", l_bmcPos, "GPIO", l_gpioName);

    // Find the GPIO line by name across all chips
    gpiod::line l_line = pgood_chassis_check::findGpioLine(l_gpioName);
    if (!l_line)
    {
        lg2::error("pgood-chassis-check: GPIO line '{GPIO}' not found, "
                   "defaulting to chassis off",
                   "GPIO", l_gpioName);
        return pgood_chassis_check::setChassisPowerState(
                   pgood_chassis_check::PowerStateIface::State::Off)
                   ? 0
                   : 1;
    }

    // Request the line for input
    if (!pgood_chassis_check::requestGpioLine(l_line, l_gpioName))
    {
        return pgood_chassis_check::setChassisPowerState(
                   pgood_chassis_check::PowerStateIface::State::Off)
                   ? 0
                   : 1;
    }

    // Read the GPIO value
    const int l_pgood = pgood_chassis_check::readGpioValue(l_line, l_gpioName);
    l_line.release();

    if (l_pgood == -1)
    {
        // Error already logged in readGpioValue; default to chassis off
        return pgood_chassis_check::setChassisPowerState(
                   pgood_chassis_check::PowerStateIface::State::Off)
                   ? 0
                   : 1;
    }

    if (l_pgood == 1)
    {
        lg2::notice(
            "pgood-chassis-check: GPIO '{GPIO}' is 1 - chassis is powered on",
            "GPIO", l_gpioName);
        return pgood_chassis_check::setChassisPowerState(
                   pgood_chassis_check::PowerStateIface::State::On)
                   ? 0
                   : 1;
    }

    lg2::info(
        "pgood-chassis-check: GPIO '{GPIO}' is 0 - chassis is powered off",
        "GPIO", l_gpioName);
    return pgood_chassis_check::setChassisPowerState(
               pgood_chassis_check::PowerStateIface::State::Off)
               ? 0
               : 1;
}
