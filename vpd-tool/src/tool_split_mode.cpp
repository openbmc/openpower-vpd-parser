#include "config.h"

#include "tool_split_mode.hpp"

#include "tool_constants.hpp"
#include "tool_error_codes.hpp"

#include <filesystem>
#include <format>
#include <iostream>

namespace vpd
{
int SplitMode::enterSplitMode(
    const std::optional<std::string>& i_filePath) const noexcept
{
    try
    {
        std::error_code l_ec;
        if (std::filesystem::exists(SYSTEM_VPD_FILE_PATH, l_ec) && !l_ec)
        {
            std::cerr
                << "CDFP cable appears to be connected to patch panel. Can't "
                   "initiate split mode configuration. Disconnect patch panel. "
                   "Unpair/factory reset BMC and retry."
                << std::endl;
            return constants::FAILURE;
        }

        const auto l_splitModeSystemVPDPath{
            constants::fileModeDirectory /
            std::filesystem::path(SYSTEM_VPD_FILE_PATH).relative_path()};

        if (i_filePath)
        {
            // Path provided — create the destination directory hierarchy and
            // copy the source file to the file mode location, overwriting any
            // existing file.
            std::filesystem::create_directories(
                l_splitModeSystemVPDPath.parent_path(), l_ec);
            if (l_ec)
            {
                std::cerr
                    << std::format(
                           "Failed to create destination path [{}] for file mode system VPD. "
                           "Reason : {}, error code [{}]. Aborting split mode environment set up.",
                           l_splitModeSystemVPDPath.parent_path().string(),
                           l_ec.message(), l_ec.value())
                    << std::endl;
                ;
                return static_cast<int>(ErrorCode::FILE_SYSTEM_ERROR);
            }

            std::filesystem::copy_file(
                i_filePath.value(), l_splitModeSystemVPDPath,
                std::filesystem::copy_options::overwrite_existing, l_ec);
            if (l_ec)
            {
                std::cerr
                    << std::format(
                           "Failed to copy system VPD file [{}] to the file mode location [{}]. "
                           "Reason : {}, error code [{}]. Aborting split mode environment set up.",
                           i_filePath.value(),
                           l_splitModeSystemVPDPath.string(), l_ec.message(),
                           l_ec.value())
                    << std::endl;
                return static_cast<int>(ErrorCode::FILE_SYSTEM_ERROR);
            }
        }
        else
        {
            // Path is not provided — verify if file already exists at the file
            // mode location.
            if (!std::filesystem::exists(l_splitModeSystemVPDPath, l_ec) ||
                l_ec)
            {
                std::cerr
                    << std::format(
                           "System VPD file not found at the file mode path [{}]. "
                           "Cannot enter split mode. Use --file to specify the system VPD file "
                           "to copy to the file mode VPD path.",
                           l_splitModeSystemVPDPath.string())
                    << std::endl;
                return static_cast<int>(ErrorCode::FILE_NOT_FOUND);
            }
        }

        // TODO - set and validate U-Boot variables
    }
    catch (const std::exception& l_ex)
    {
        std::cerr
            << std::format(
                   "Exception occured while setting system in split mode, error : {}. "
                   "Aborting split mode environment set up.",
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
        //  2. set U-Boot variables "fieldmmode" as false and "vpdmode" to
        //  hardware.
        //  3. Validate if U-Boot variables are correctly set.
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
