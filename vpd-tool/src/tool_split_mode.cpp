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

std::expected<std::tuple<std::string, std::string>, ErrorCode>
    SplitMode::getUbootVariableValues() const noexcept
{
    const auto l_fieldModeOutput =
        utils::executeCmd(std::format("fw_printenv {}", constants::fieldMode));
    if (!l_fieldModeOutput)
    {
        std::cerr << std::format("Failed to read {} variable value.\n",
                                 constants::fieldMode);
        return std::unexpected(l_fieldModeOutput.error());
    }

    const auto l_vpdModeOutput =
        utils::executeCmd(std::format("fw_printenv {}", constants::vpdMode));
    if (!l_vpdModeOutput)
    {
        std::cerr << std::format("Failed to read {} variable value.\n",
                                 constants::vpdMode);
        return std::unexpected(l_vpdModeOutput.error());
    }

    // fw_printenv outputs "<var>=<value>\n"; extract the value after "=".
    auto l_parseValue = [](const std::vector<std::string>& l_cmdOutput,
                           const std::string& l_varName) -> std::string {
        if (l_cmdOutput.empty())
        {
            return {};
        }

        std::string l_ubootVarValue = l_cmdOutput.front();

        // Remove the new line character from the string.
        l_ubootVarValue.erase(l_ubootVarValue.length() - 1);
        const auto& l_equalPos = l_ubootVarValue.find("=");

        if (l_equalPos == std::string::npos)
        {
            std::cerr << std::format(
                "Unexpected output from fw_printenv for [{}]: [{}].", l_varName,
                l_ubootVarValue);
        }

        return l_ubootVarValue.substr(l_equalPos + 1);
    };

    return std::make_tuple(
        l_parseValue(l_fieldModeOutput.value(), constants::fieldMode),
        l_parseValue(l_vpdModeOutput.value(), constants::vpdMode));
}

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

        // set up U-Boot variables filedmode=false and vpdmode=file. So after
        // reboot BMC can read system VPD from file mode path.
        const auto& l_setUBootVariableStatus =
            setUbootVariables("false", "file");
        if (!l_setUBootVariableStatus)
        {
            std::cerr
                << "Failed to set U-Boot environment variables. Aborting split mode environment set up."
                << std::endl;
            return static_cast<int>(l_setUBootVariableStatus.error());
        }

        const auto l_getUBootVariableStatus = getUbootVariableValues();

        if (!l_getUBootVariableStatus)
        {
            std::cerr
                << "Failed to validate U-Boot environment variables. Aborting split mode environment setup."
                << std::endl;
            return static_cast<int>(l_getUBootVariableStatus.error());
        }

        const auto& [l_fieldMode, l_vpdMode] = l_getUBootVariableStatus.value();

        if (l_fieldMode != "false")
        {
            std::cerr
                << std::format(
                       "Field mode is set to [{}], expected [false]. Cannot enter split mode.",
                       l_fieldMode)
                << std::endl;
            return constants::FAILURE;
        }

        if (l_vpdMode != "file")
        {
            std::cerr << std::format(
                             "VPD mode is set to [{}], expected [file]. "
                             "Cannot enter split mode.",
                             l_vpdMode)
                      << std::endl;
            return constants::FAILURE;
        }

        std::cout
            << std::format(
                   "Split mode environment setup completed. Reboot the BMC with "
                   "CDFP cables disconnected to start the BMC in split mode.\n"
                   "Note: To update a keyword in the system VPD file, use the "
                   "following vpd-tool command:\n\n"
                   "Usage: vpd-tool -w -H -O /var/lib/vpd/file{} "
                   "-R <record_name> -K <keyword_name> -V <value>",
                   SYSTEM_VPD_FILE_PATH)
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
