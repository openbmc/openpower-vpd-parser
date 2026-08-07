// SPDX-License-Identifier: Apache-2.0

/**
 * pgood-chassis-check
 *
 * Reads the power-good GPIO for the local chassis and decides the role
 * in which wait-vpd-parsers.service should run.
 *
 * BMC position is read from /run/openbmc/bmc_position:
 *   0 (or file missing/unreadable) -> read GPIO "power-good-chassis1"
 *   1                               -> read GPIO "power-good-chassis2"
 *
 * Outcome written to /run/wait-vpd-role (an environment file):
 *   GPIO=0 (chassis off):  WAIT_VPD_ROLE=active  -> full VPD collection
 *   GPIO=1 (chassis on):   WAIT_VPD_ROLE=poweron -> Status=Completed only,
 *                          no VPD collection attempted while chassis is live.
 *
 * wait-vpd-parsers.service picks up this file via EnvironmentFile= and
 * passes ${WAIT_VPD_ROLE} to wait-vpd-parser --role.
 */

#include "constants.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

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
 * @brief Write the VPD role environment file consumed by
 *        wait-vpd-parsers.service.
 *
 * Writes "WAIT_VPD_ROLE=<role>" to @c vpdRoleFile so that systemd's
 * EnvironmentFile= directive makes ${WAIT_VPD_ROLE} available to the
 * wait-vpd-parser --role argument.
 *
 * @param[in] i_role Role string: "active" or "passive".
 * @return true on success, false on failure. All exceptions are caught
 *         locally.
 */
bool writeVpdRoleFile(const std::string& i_role) noexcept
{
    try
    {
        std::ofstream l_roleFile(vpdRoleFile);
        if (!l_roleFile)
        {
            lg2::error("pgood-chassis-check: failed to create role file "
                       "'{FILE}': {ERR}",
                       "FILE", vpdRoleFile, "ERR", std::strerror(errno));
            return false;
        }
        l_roleFile << "WAIT_VPD_ROLE=" << i_role << "\n";
        return true;
    }
    catch (const std::exception& l_ex)
    {
        lg2::error("pgood-chassis-check: exception writing role file "
                   "'{FILE}': {ERR}",
                   "FILE", vpdRoleFile, "ERR", l_ex.what());
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
                   "defaulting to active role",
                   "GPIO", l_gpioName);
        return pgood_chassis_check::writeVpdRoleFile(
                   pgood_chassis_check::roleActive)
                   ? 0
                   : 1;
    }

    // Request the line for input
    if (!pgood_chassis_check::requestGpioLine(l_line, l_gpioName))
    {
        return pgood_chassis_check::writeVpdRoleFile(
                   pgood_chassis_check::roleActive)
                   ? 0
                   : 1;
    }

    // Read the GPIO value
    const int l_pgood = pgood_chassis_check::readGpioValue(l_line, l_gpioName);
    l_line.release();

    if (l_pgood == -1)
    {
        // Error already logged in readGpioValue; default to active role
        return pgood_chassis_check::writeVpdRoleFile(
                   pgood_chassis_check::roleActive)
                   ? 0
                   : 1;
    }

    if (l_pgood == 1)
    {
        lg2::notice("pgood-chassis-check: GPIO '{GPIO}' is 1 - chassis is "
                    "powered on, setting wait-vpd-parsers to poweron role "
                    "(Status=Completed only, no VPD collection)",
                    "GPIO", l_gpioName);
        return pgood_chassis_check::writeVpdRoleFile(
                   pgood_chassis_check::rolePoweron)
                   ? 0
                   : 1;
    }

    lg2::info("pgood-chassis-check: GPIO '{GPIO}' is 0 - chassis is off, "
              "setting wait-vpd-parsers to active role (full VPD collection)",
              "GPIO", l_gpioName);
    return pgood_chassis_check::writeVpdRoleFile(
               pgood_chassis_check::roleActive)
               ? 0
               : 1;
}
