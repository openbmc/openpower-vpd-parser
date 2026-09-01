#include "config.h"

#include "tool_split_mode.hpp"

#include "tool_constants.hpp"

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
            std::cerr << "Error: Cable connected, can't enter split mode."
                      << std::endl;
            return constants::FAILURE;
        }

        if (i_filePath)
        {
            // Path provided — create the destination directory hierarchy and
            // copy the source file to the file mode location, overwriting any
            // existing file.
            const std::filesystem::path l_dest{
                constants::splitModeSystemVpdPath};

            std::filesystem::create_directories(l_dest.parent_path(), l_ec);
            if (l_ec)
            {
                std::cerr << std::format(
                                 "Error: Failed to create directory [{}]: {}",
                                 l_dest.parent_path().string(), l_ec.message())
                          << std::endl;
                return static_cast<int>(ErrorCode::FILE_SYSTEM_ERROR);
            }

            std::filesystem::copy_file(
                i_filePath.value(), l_dest,
                std::filesystem::copy_options::overwrite_existing, l_ec);
            if (l_ec)
            {
                std::cerr << std::format(
                                 "Error: Failed to copy [{}] to [{}]: {}",
                                 i_filePath.value(), l_dest.string(),
                                 l_ec.message())
                          << std::endl;
                return static_cast<int>(ErrorCode::FILE_SYSTEM_ERROR);
            }
        }
        else
        {
            // No path provided — verify the file already exists at the file
            // mode location.
            if (!std::filesystem::exists(constants::splitModeSystemVpdPath,
                                         l_ec) ||
                l_ec)
            {
                std::cerr << std::format(
                                 "Error: Need system VPD file to continue. "
                                 "File not found at [{}].",
                                 constants::splitModeSystemVpdPath)
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
