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
     * - If i_filePath is provided copys the system VPD from the given path to the file mode path. If path is not provided assumes the path is already present at the file mode path.
     * - Sets the U-Boot variables 'fieldmode' and 'vpdmode'
     * - Validates if the U-Boot variables are set.
     * - If required, updates the Serial number of system VPD.
     *
     * @param[in] i_filePath - Path to copy system VPD.
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
