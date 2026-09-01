#pragma once

#include "tool_error_codes.hpp"

#include <expected>
#include <optional>
#include <string>
#include <tuple>

namespace vpd
{

/**
 * @brief Class to support split mode operations.
 *
 * System VPD EEPROM is not accessible in split mode. So, system
 * VPD is copied to the file mode loaction before entering split mode. This
 * allows BMC to read the VPD from the file mode path after reboot. To read
 * the system VPD from file mode path requires setting up U-Boot variables.
 *
 * The class provides support to enter and exit split mode.
 *
 */
class SplitMode
{
  private:
    /**
     * @brief Set U-Boot environment variables
     *
     * This API sets the U-Boot variables "fieldmode" and "vpdmode" with the
     * given values.
     *
     * @param[in] i_fieldModeValue - Value to set for fieldmode variable.
     * @param[in] i_vpdModeValue   - Value to set for vpdmode variable.
     *
     * @return On success returns void, corresponding error code on failure.
     */
    std::expected<void, ErrorCode> setUbootVariables(
        const std::string& i_fieldModeValue,
        const std::string& i_vpdModeValue) const noexcept;

    /**
     * @brief Read U-Boot environment variables
     *
     * Reads the U-Boot environment variables for field mode and VPD mode and
     * returns their values.
     *
     * @return On success returns a tuple of (fieldmode value, vpdmode value),
     * corresponding error code on failure.
     */
    std::expected<std::tuple<std::string, std::string>, ErrorCode>
        getUbootVariableValues() const noexcept;

  public:
    /**
     * @brief Set up the system in split mode.
     *
     * Setting up the system in split mode requires system configuartion
     * changes. Performs the following actions:
     * 1. Check if the system VPD file path is accessible.
     * 2. Copy the system VPD file to the file-mode location if a source path is
     * provided; otherwise, verify that the file exists at the file-mode path.
     * 3. Set the U-Boot environment variables "fieldmmode" as false and
     * "vpdmode" to file.
     * 4. Validate that the U-Boot environment variables are correctly set.
     *
     * @param[in] i_filePath - Path of the source file to be copied(Optional).
     *
     * @return 0 on success. Corresponding error code on failure.
     */
    int enterSplitMode(
        const std::optional<std::string>& i_filePath) const noexcept;

    /**
     * @brief API to exit split mode.
     *
     * Resets the system changes done while setting up the system in split mode.
     * Performs the following tasks -
     * - Removes the system file copied to file mode location.
     * - Set U-Boot variables "fieldmmode" as false and "vpdmode" to hardware.
     *
     * @return 0 on success. Corresponding error code on failure.
     */
    int exitSplitMode() const noexcept;
};
} // namespace vpd
