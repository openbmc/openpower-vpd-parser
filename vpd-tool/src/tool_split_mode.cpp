#include "tool_split_mode.hpp"

#include "tool_constants.hpp"
#include "tool_utils.hpp"

#include <expected>
#include <filesystem>
#include <format>
#include <iostream>

namespace vpd
{
std::expected<void, ErrorCode> SplitMode::setUbootVar(const std::string& i_fieldModeValue,
                             const std::string& i_vpdModeValue) const noexcept
{
    if (const auto& l_cmdStatus = utils::executeCmd(std::format("fw_setenv fieldmode {}", i_fieldModeValue)); !l_cmdStatus)
    {
        std::cerr << std::format("Failed to set fieldmode to {}.\n", i_fieldModeValue);
        return std::unexpected(l_cmdStatus.error());
    }

    if (const auto& l_cmdStatus = utils::executeCmd(std::format("fw_setenv vpdmode {}", i_vpdModeValue)); !l_cmdStatus)
    {
        std::cerr << std::format("Failed to set vpdmode to {}.\n", i_vpdModeValue);
        return std::unexpected(l_cmdStatus.error());
    }

    return {};
}

std::expected<std::tuple<std::string, std::string>, ErrorCode>
    SplitMode::getUbootVar() const noexcept
{
    const auto l_fieldModeOutput = utils::executeCmd(
        std::format("fw_printenv {}", constants::fieldMode));
    if (!l_fieldModeOutput)
    {
        std::cerr << std::format("Failed to get {}.\n",
                                 constants::fieldMode);
        return std::unexpected(l_fieldModeOutput.error());
    }

    const auto l_vpdModeOutput = utils::executeCmd(
        std::format("fw_printenv {}", constants::vpdMode));
    if (!l_vpdModeOutput)
    {
        std::cerr << std::format("Failed to get {}.\n",
                                 constants::vpdMode);
        return std::unexpected(l_vpdModeOutput.error());
    }

    // fw_printenv outputs "<var>=<value>\n"; extract the value after "=".
    auto l_parseValue = [](const std::vector<std::string>& i_lines,
                           const std::string& i_varName) -> std::string {
        if (i_lines.empty())
        {
            return {};
        }
        const std::string l_prefix = i_varName + "=";
        const std::string& l_line = i_lines.front();
        if (l_line.substr(0, l_prefix.size()) == l_prefix)
        {
            std::string l_val = l_line.substr(l_prefix.size());
            if (!l_val.empty() && l_val.back() == '\n')
            {
                l_val.pop_back();
            }
            return l_val;
        }
        std::cerr << std::format(
            "Unexpected output from fw_printenv for [{}]: [{}].", i_varName,
            l_line);
        return {};
    };

    return std::make_tuple(
        l_parseValue(l_fieldModeOutput.value(), constants::fieldMode),
        l_parseValue(l_vpdModeOutput.value(), constants::vpdMode));
}

int SplitMode::setUpSplitMode(const std::string& i_filePath) const noexcept
{
    try
    {
        std::error_code l_ec;
        if (std::filesystem::exists(constants::systemVpdPath, l_ec) &&
            !l_ec)
        {
            std::cerr << "Error: Cable connected, can't enter split mode."
                      << std::endl;
            return constants::FAILURE;
        }

        // Step 2: Handle VPD file.
        if (!i_filePath.empty())
        {
            // Path provided — create the destination directory hierarchy and
            // copy the source file to the file mode location, overwriting any
            // existing file.
            const std::filesystem::path l_dest{constants::splitModeSystemVpdPath};

            std::filesystem::create_directories(l_dest.parent_path(), l_ec);
            if (l_ec)
            {
                std::cerr << std::format(
                    "Error: Failed to create directory [{}]: {}",
                    l_dest.parent_path().string(), l_ec.message())
                          << std::endl;
                return constants::FAILURE;
            }

            std::filesystem::copy_file(
                i_filePath, l_dest,
                std::filesystem::copy_options::overwrite_existing, l_ec);
            if (l_ec)
            {
                std::cerr << std::format(
                    "Error: Failed to copy [{}] to [{}]: {}", i_filePath,
                    l_dest.string(), l_ec.message())
                          << std::endl;
                return constants::FAILURE;
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
                return constants::FAILURE;
            }
        }

        if (const auto& l_setUBootVarialbeStatus = setUbootVar("false", "file"); !l_setUBootVarialbeStatus)
        {
            return static_cast<int>(l_setUBootVarialbeStatus.error());
        }

        const auto l_getUBootVariableStatus = getUbootVar();

        if (!l_getUBootVariableStatus)
        {
            return static_cast<int>(l_getUBootVariableStatus.error());
        }

        if (std::get<0>(l_getUBootVariableStatus.value()) != "false")
        {
            std::cerr << "Field mode value is not false. Exitig" << std::endl;
            return constants::FAILURE;
        }

        if (std::get<1>(l_getUBootVariableStatus.value()) != "file")
        {
            std::cerr << "VPD mode is not in file mode." << std::endl;
            return constants::FAILURE;
        }

        // Step 5: Prompt user to optionally update the system serial number.
        constexpr auto MAX_ANSWER_LEN{3};
        std::string l_answer{};
        std::cout << "Update serial number of the system? [yes/no]: ";
        std::cin >> std::setw(MAX_ANSWER_LEN) >> l_answer;

        if (l_answer == "yes")
        {
            std::string l_serialNumberValue{};
            std::cout << "Enter new Serial Number: ";
            std::cin >> l_serialNumberValue;

            if (l_serialNumberValue.empty())
            {
                std::cerr << "Error: Empty serial number provided." << std::endl;
                return constants::FAILURE;
            }

            const auto& l_binaryValue = utils::convertToBinary(l_serialNumberValue);

            if (!l_binaryValue.has_value())
            {
                return static_cast<int>(l_binaryValue.error());
            }

            const auto l_rc = utils::writeKeywordOnHardware(constants::splitModeSystemVpdPath, std::make_tuple("VSYS", "SE", l_binaryValue.value()));

            if (l_rc <= constants::SUCCESS)
            {
                std::cerr << "Failed to update Serial number" << std::endl;
                return constants::FAILURE;
            }
        }

        std::cout
            << "\nAll settings done. Reboot BMC with CDFP cable disconnected "
               "to start BMC in split mode."
            << std::endl;

        return constants::SUCCESS;
    }
    catch (const std::exception& l_ex)
    {
        std::cerr << std::format("Exception in setUpSplitMode: {}", l_ex.what())
                  << std::endl;
        return constants::FAILURE;
    }
}

int SplitMode::exitSplitMode() const noexcept
{
    std::error_code l_ec;
    if (std::filesystem::exists(constants::splitModeSystemVpdPath, l_ec))
    {
        if (l_ec)
        {
            std::cerr << std::format("Failed to check if file mode VPD file path [{}] exists. Error : {}.", constants::splitModeSystemVpdPath, l_ec.message()) << std::endl;
        }

        std::filesystem::remove_all(constants::fileModeDirectory, l_ec);

        if (l_ec)
        {
            std::cerr << std::format("Failed to delete file mode system VPD file path [{}]. Error : {}.", constants::fileModeDirectory, l_ec.message()) << std::endl;
        }
    }

    if (const auto& l_setUBootVarialbeStatus = setUbootVar("false", "hardware"); !l_setUBootVarialbeStatus)
    {
        return static_cast<int>(l_setUBootVarialbeStatus.error());
    }

    const auto l_getUBootVariableStatus = getUbootVar();

    if (!l_getUBootVariableStatus)
    {
        return static_cast<int>(l_getUBootVariableStatus.error());
    }

    if (std::get<0>(l_getUBootVariableStatus.value()) != "false")
    {
        std::cerr << "Field mode value is not false. Exit" << std::endl;
        return constants::FAILURE;
    }

    if (std::get<1>(l_getUBootVariableStatus.value()) != "hardware")
    {
        std::cout << std::get<1>(l_getUBootVariableStatus.value());
        std::cerr << "VPD mode is not in hardware mode. Exit" << std::endl;
        return constants::FAILURE;
    }

    std::cout << "Exit split mode process completed. Proceed with factory reset and boot the BMC with CDFP connected." << std::endl;

    return constants::SUCCESS;
}
} // namespace vpd
