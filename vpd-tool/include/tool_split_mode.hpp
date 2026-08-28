#pragma once

#include <string>

namespace vpd
{
/**
 * @brief Class to support split mode operations.
 *
 * The class provides API's to enter and exit split mode on the system.
 */
class SplitMode
{
  public:
    /**
     * @brief API to setup system in split mode
     *
     * This API sets the system in split mode.
     * Performs the following actions to set the system in split mode:
     * 1. Check if system VPD file path is accessible.
     * 2. Copy to file mode location if path given, else verify if file is
     * present at file mode path.
     * 3. Set both U-Boot environment variables for split mode.
     * 4. Validate the U-Boot variables are correctly set.
     * 5. Update the system serial number if required.
     *
     * @param[in] i_filePath - Path to the source file to be copied.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int setUpSplitMode(const std::string& i_filePath = {}) const noexcept;

    /**
     * @brief Exit split mode.
     *
     * API to exit split mode on the system.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int exitSplitMode() const noexcept;
};
} // namespace vpd
