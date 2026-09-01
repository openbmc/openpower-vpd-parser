#include "config.h"

#include "tool_split_mode.hpp"

#include "tool_constants.hpp"
#include "tool_error_codes.hpp"
#include "tool_utils.hpp"

#include <filesystem>
#include <format>
#include <iostream>

namespace vpd
{
std::expected<void, ErrorCode> SplitMode::setUbootVariables(
    const std::string& i_fieldModeValue,
    const std::string& i_vpdModeValue) const noexcept
{
    try
    {
        if (const auto& l_cmdStatus = utils::executeCmd(
                std::format("fw_setenv fieldmode {}", i_fieldModeValue));
            !l_cmdStatus)
        {
            std::cerr << std::format("Failed to set fieldmode to {}.\n",
                                     i_fieldModeValue);
            return std::unexpected(l_cmdStatus.error());
        }

        if (const auto& l_cmdStatus = utils::executeCmd(
                std::format("fw_setenv vpdmode {}", i_vpdModeValue));
            !l_cmdStatus)
        {
            std::cerr << std::format("Failed to set vpdmode to {}.\n",
                                     i_vpdModeValue);
            return std::unexpected(l_cmdStatus.error());
        }
    }
    catch (const std::exception& l_ex)
    {
        std::cerr
            << std::format(
                   "Exception occured while setting up U-Boot variables. Error : {}.",
                   l_ex.what())
            << std::endl;
        return std::unexpected(ErrorCode::STANDARD_EXCEPTION);
    }
    return {};
}

int SplitMode::enterSplitMode(
    const std::optional<std::string>& i_filePath) const noexcept
{
    try
    {
        std::error_code l_ec;
        if (std::filesystem::exists(SYSTEM_VPD_FILE_PATH, l_ec) && !l_ec)
        {
            std::cerr << "Error: Cable is connected, can't enter split mode."
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
                           "Error: Failed to create directory [{}]: {}",
                           l_splitModeSystemVPDPath.parent_path().string(),
                           l_ec.message())
                    << std::endl;
                ;
                return static_cast<int>(ErrorCode::FILE_SYSTEM_ERROR);
            }

            std::filesystem::copy_file(
                i_filePath.value(), l_splitModeSystemVPDPath,
                std::filesystem::copy_options::overwrite_existing, l_ec);
            if (l_ec)
            {
                std::cerr << std::format(
                                 "Error: Failed to copy [{}] to [{}]: {}",
                                 i_filePath.value(),
                                 l_splitModeSystemVPDPath.string(),
                                 l_ec.message())
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
                std::cerr << std::format(
                                 "Error: Need system VPD file to continue. "
                                 "File not found at [{}].",
                                 l_splitModeSystemVPDPath.string())
                          << std::endl;
                return static_cast<int>(ErrorCode::FILE_NOT_FOUND);
            }
        }

        // set up U-Boot variables filedmode=false and vpdmode=file. So after
        // reboot BMC can read system VPD from file mode path.
        const auto& l_setUBootVarialbeStatus =
            setUbootVariables("false", "file");
        if (!l_setUBootVarialbeStatus)
        {
            return static_cast<int>(l_setUBootVarialbeStatus.error());
        }

        // TODO - validate U-Boot variables.
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
