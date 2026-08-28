#include "tool_split_mode.hpp"

#include "tool_constants.hpp"

namespace vpd
{
int SplitMode::setUpSplitMode(const std::string& i_filePath) const noexcept
{
    (void)i_filePath;
    // TODO: Implement the following steps:
    // 1. Check if system VPD file path is accessible.
    // 2. Handle VPD file: copy to file mode location if path given, else verify
    //    if file is present at file mode path.
    // 3. Set both U-Boot environment variables for split mode.
    // 4. Validate the U-Boot variables are correctly set.
    // 5. Update the system serial number if required.

    return constants::SUCCESS;
}

int SplitMode::exitSplitMode() const noexcept
{
    // TODO: Implement exit split mode logic.

    return constants::SUCCESS;
}
} // namespace vpd
