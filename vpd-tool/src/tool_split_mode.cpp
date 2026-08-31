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
int SplitMode::enterSplitMode(
    const std::optional<std::string>& i_filePath) const noexcept
{
    try
    {
        std::error_code l_ec;
        const auto l_systemVpdPathExists =
            std::filesystem::exists(SYSTEM_VPD_FILE_PATH, l_ec);

        if (l_ec)
        {
            std::cerr
                << std::format(
                       "Failed to check system VPD [{}] accessibility, reason : {}, error code [{}]. Aborting split mode environment set up.",
                       SYSTEM_VPD_FILE_PATH, l_ec.message(), l_ec.value())
                << std::endl;
            return static_cast<int>(ErrorCode::FILE_SYSTEM_ERROR);
        }

        if (l_systemVpdPathExists)
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
            const auto l_fileModePathExists =
                std::filesystem::exists(l_splitModeSystemVPDPath, l_ec);

            if (l_ec)
            {
                std::cerr
                    << std::format(
                           "Failed to check file mode system VPD path [{}] exists, reason : {}, error code [{}]. Aborting split mode environment set up.",
                           l_splitModeSystemVPDPath.string(), l_ec.message(),
                           l_ec.value())
                    << std::endl;
                return static_cast<int>(ErrorCode::FILE_SYSTEM_ERROR);
            }

            if (!l_fileModePathExists)
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

        const auto l_ubootFieldModeSetResult = utils::setAndValidateUbootVar(
            constants::ubootVarFieldMode, constants::ubootValFieldModeFalse);

        if (!l_ubootFieldModeSetResult || !(*l_ubootFieldModeSetResult))
        {
            std::cerr
                << std::format(
                       "Failed to set U-boot variable [{}] to [{}]. Aborting split mode setup.",
                       constants::ubootVarFieldMode,
                       constants::ubootValFieldModeFalse)
                << std::endl;
            return (!l_ubootFieldModeSetResult)
                       ? static_cast<int>(l_ubootFieldModeSetResult.error())
                       : constants::FAILURE;
        }

        const auto l_ubootVpdModeSetResult = utils::setAndValidateUbootVar(
            constants::ubootVarVpdMode, constants::ubootValVpdModeFile);

        if (!l_ubootVpdModeSetResult || !(*l_ubootVpdModeSetResult))
        {
            std::cerr
                << std::format(
                       "Failed to set U-boot variable [{}] to [{}]. Aborting split mode setup.",
                       constants::ubootVarVpdMode,
                       constants::ubootValVpdModeFile)
                << std::endl;
            return (!l_ubootVpdModeSetResult)
                       ? static_cast<int>(l_ubootVpdModeSetResult.error())
                       : constants::FAILURE;
        }

        std::cout
            << std::format(
                   "Split mode environment set up is complete.\n"
                   "Next steps:\n"
                   "  1. If any VPD record/keyword in the system VPD file needs to be updated, update before rebooting\n"
                   "     use the following command: \n"
                   "       vpd-tool -w -H -O \"{}\" -R <record_name> -K <keyword_name> -V <value_to_update>\n"
                   "  2. With CDFP cable disconnected, reboot the BMC to start in split mode",
                   l_splitModeSystemVPDPath.string())
            << std::endl;
    }
    catch (const std::exception& l_ex)
    {
        std::cerr
            << std::format(
                   "Exception occured while setting system in split mode, error : {}."
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
        std::error_code l_ec;

        // As part of split-mode setup, the file-mode directory is created and
        // the system VPD is copied to the file-mode path. Since the system is
        // exiting split mode, the file-mode directory and its contents are no
        // longer required.
        std::filesystem::remove_all(constants::fileModeDirectory, l_ec);

        if (l_ec)
        {
            std::cerr << std::format(
                             "Failed to remove file-mode directory [{}]."
                             "Error: {}, error code [{}].",
                             constants::fileModeDirectory, l_ec.message(),
                             l_ec.value())
                      << std::endl;
            // Note: Continuing despite the error, as the presence of the file
            // does not cause any issues in normal mode.
        }

        const auto l_ubootFieldModeSetResult = utils::setAndValidateUbootVar(
            constants::ubootVarFieldMode, constants::ubootValFieldModeFalse);

        if (!l_ubootFieldModeSetResult || !(*l_ubootFieldModeSetResult))
        {
            std::cerr << std::format("U-boot variable [{}] is not set to [{}].",
                                     constants::ubootVarFieldMode,
                                     constants::ubootValFieldModeFalse)
                      << std::endl;
            // Note: Continuing despite the error, as the field-mode value does
            // not impact normal mode.
        }

        const auto l_ubootVpdModeSetResult = utils::setAndValidateUbootVar(
            constants::ubootVarVpdMode, constants::ubootValVpdModeHardware);

        if (!l_ubootVpdModeSetResult || !(*l_ubootVpdModeSetResult))
        {
            std::cerr
                << std::format(
                       "Failed to set U-boot variable [{}] to [{}]. Aborting exit split mode.",
                       constants::ubootVarVpdMode,
                       constants::ubootValVpdModeHardware)
                << std::endl;

            return (!l_ubootVpdModeSetResult)
                       ? static_cast<int>(l_ubootVpdModeSetResult.error())
                       : constants::FAILURE;
        }

        std::cout
            << "Environment is set to exit split mode.\n"
               "Do factory reset and boot the BMC with CDFP cables connected."
            << std::endl;
    }
    catch (const std::exception& l_ex)
    {
        std::cerr
            << std::format(
                   "Exception occured while exiting split mode. Error : {}. Aborting exit split mode.",
                   l_ex.what())
            << std::endl;

        return static_cast<int>(ErrorCode::STANDARD_EXCEPTION);
    }

    return constants::SUCCESS;
}
} // namespace vpd
