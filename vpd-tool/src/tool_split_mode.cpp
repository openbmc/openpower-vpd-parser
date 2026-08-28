#include "tool_split_mode.hpp"

#include "tool_constants.hpp"

#include <format>
#include <iostream>

namespace vpd
{
int SplitMode::enterSplitMode(
    const std::optional<std::string>& i_filePath) const noexcept
{
    try
    {
        (void)i_filePath;
        // TODO: Implement the following steps:
        // 1. Check if system VPD file path is accessible.
        // 2. copy to file mode location if path given, else verify
        //    if file is present at file mode location.
        // 3. Set both U-Boot environment variables for split mode.
        // 4. Validate the U-Boot variables are correctly set.
    }
    catch (const std::exception& l_ex)
    {
        std::cerr
            << std::format(
                   "Exception occured while setting system in split mode. Error : {}",
                   l_ex.what())
            << std::endl;
        return constants::FAILURE;
    }

    return constants::SUCCESS;
}

int SplitMode::exitSplitMode() const noexcept
{
    try
    {
        // TODO: Inplement the following steps:
        //  1. If system VPD file is present at the file mode location delete
        //  the path.
        //  2. Reset U-Boot variables.
    }
    catch (const std::exception& l_ex)
    {
        std::cerr
            << std::format(
                   "Exception occured while exiting split mode. Error : {}",
                   l_ex.what())
            << std::endl;
        return constants::FAILURE;
    }
    return constants::SUCCESS;
}
} // namespace vpd
