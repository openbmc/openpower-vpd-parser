#pragma once

#include "tool_error_codes.hpp"

#include <expected>
#include <string>
#include <tuple>

namespace vpd
{

/**
 * @brief Class to support split mode operations.
 *
 * The class provides support to enter and exit split mode on the system.
 */
class SplitMode
{
  private:
    /**
     * @brief Set U-Boot environment variables for split mode
     *
     * @param[in] i_fieldModeValue - Value to set for fieldmode variable.
     * @param[in] i_vpdModeValue   - Value to set for vpdmode variable.
     *
     * @return On success returns void, corresponding error code on failure.
     */
    std::expected<void, ErrorCode> setUbootVar(
        const std::string& i_fieldModeValue,
        const std::string& i_vpdModeValue) const noexcept;

    /**
     * @brief Read U-Boot environment variables for split mode
     *
     * @return On success returns a tuple of (fieldmode value, vpdmode value),
     * corresponding error code on failure.
     */
    std::expected<std::tuple<std::string, std::string>, ErrorCode>
        getUbootVar() const noexcept;

  public:
    /**
     * @brief Set up the system in split mode.
     *
     * Setting up the system in split mode requires system changes.
     * Since the system VPD EEPROM is not accessible in split mode, the system
     * VPD is copied to the file-mode loaction before entering split mode. This
     * allows to read the VPD from the file-mode path after reboot.
     *
     * Performs the following actions:
     * 1. Check if the system VPD file path is accessible.
     * 2. Copy the system VPD file to the file-mode location if a source path is
     * provided; otherwise, verify that the file exists at the file-mode path.
     * 3. Set the U-Boot environment variables required for split mode.
     * 4. Validate that the U-Boot environment variables are correctly set.
     * 5. Update the system serial number, if required.
     *
     * @param[in] i_filePath Path of the source file to be copied.
     *
     * @return 0 on success; -1 otherwise.
     */
    int enterSplitMode(const std::string& i_filePath = {}) const noexcept;

    /**
     * @brief API to exit split mode.
     *
     * Resets the system changes done while setting up system in split mode.
     * Performs the following tasks -
     * - Removes the system file copied to file mode loaction.
     * - Reset the UBoot variables
     *
     * @return 0 on success; -1 otherwise.
     */
    int exitSplitMode() const noexcept;
};
} // namespace vpd
