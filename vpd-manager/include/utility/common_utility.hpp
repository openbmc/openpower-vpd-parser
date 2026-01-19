#pragma once

#include "constants.hpp"
#include "error_codes.hpp"
#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

/**
 * @brief Namespace to host common utility methods.
 *
 * A method qualifies as a common utility function if,
 * A)It is being used by the utility namespace at the same level as well as
 * other files directly.
 * B) The utility should be a leaf node and should not be dependent on any other
 * utility.
 *                  *******************
 *                  | Commmon Utility | - - - - - - -
 *                  *******************              |
 *                          /\                       |
 *                         /  \                      |
 *         ****************    ****************      |
 *         | json utility |    | dbus utility |      |
 *         ****************    ****************      |
 *                 \                 /               |
 *                  \               /                |
 *               ************************            |
 *               | Vpd specific Utility | - - - - - - -
 *               ************************
 */

namespace vpd
{

namespace commonUtility
{
/**
 * @brief API to get error code message.
 *
 * @param[in] i_errCode - error code.
 *
 * @return Error message set for that error code. Otherwise empty
 * string.
 */
inline std::string getErrCodeMsg(const uint16_t& i_errCode)
{
    if (errorCodeMap.find(i_errCode) != errorCodeMap.end())
    {
        return errorCodeMap.at(i_errCode);
    }

    return std::string{};
}

/** @brief Return the hex representation of the incoming byte.
 *
 * @param [in] i_aByte - The input byte.
 * @returns Null character if input byte is out of bound else returns hex
 * representation of the byte as a character.
 */
constexpr auto toHex(const size_t& i_aByte) noexcept
{
    constexpr auto l_map = "0123456789abcdef";

    return (i_aByte < std::strlen(l_map)) ? l_map[i_aByte] : '\0';
}

/**
 * @brief API to return null at the end of variadic template args.
 *
 * @return empty string.
 */
inline std::string getCommand()
{
    return "";
}

/**
 * @brief API to arrange create command.
 *
 * @param[in] arguments to create the command
 * @return Command string
 */
template <typename T, typename... Types>
inline std::string getCommand(T i_arg1, Types... i_args)
{
    std::string l_cmd = " " + i_arg1 + getCommand(i_args...);

    return l_cmd;
}

/**
 * @brief API to create shell command and execute.
 *
 * @throw std::runtime_error.
 *
 * @param[in] arguments for command
 * @param[out] o_errCode - To set error code in case of error.
 * @returns output of that command
 */
template <typename T, typename... Types>
inline std::vector<std::string> executeCmd(T&& i_path, uint16_t& o_errCode,
                                           Types... i_args)
{
    o_errCode = 0;
    std::vector<std::string> l_cmdOutput;

    try
    {
        std::array<char, constants::CMD_BUFFER_LENGTH> l_buffer;

        std::string l_cmd = i_path + getCommand(i_args...);

        std::unique_ptr<FILE, decltype(&pclose)> l_cmdPipe(
            popen(l_cmd.c_str(), "r"), pclose);

        if (!l_cmdPipe)
        {
            o_errCode = error_code::POPEN_FAILED;
            Logger::getLoggerInstance()->logMessage(
                "popen failed with error " + std::string(strerror(errno)));
            return l_cmdOutput;
        }

        while (fgets(l_buffer.data(), l_buffer.size(), l_cmdPipe.get()) !=
               nullptr)
        {
            l_cmdOutput.emplace_back(l_buffer.data());
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Error while trying to execute command [" + std::string(i_path) +
            "], error : " + std::string(l_ex.what()));
    }

    return l_cmdOutput;
}

/** @brief Converts string to lower case.
 *
 * @param [in] i_string - Input string.
 */
inline void toLower(std::string& i_string)
{
    std::transform(i_string.begin(), i_string.end(), i_string.begin(),
                   [](unsigned char l_char) { return std::tolower(l_char); });
}

/**
 * @brief An API to get hex representation of the incoming bytes.
 *
 * The API returns the hex represented value of the given input in string format
 * with 0x prefix.
 *
 * @param[in] i_keywordValue - Vector of input byte.
 *
 * @return - Returns the converted string value.
 */
inline std::string convertByteVectorToHex(
    const types::BinaryVector& i_keywordValue)
{
    std::ostringstream l_oss;
    l_oss << "0x";
    for (const auto& l_byte : i_keywordValue)
    {
        l_oss << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<int>(l_byte);
    }

    return l_oss.str();
}

/**
 * @brief An API to convert binary value into ascii/hex representation.
 *
 * If given data contains printable characters, ASCII formated string value of
 * the input data will be returned. Otherwise if the data has any non-printable
 * value, returns the hex represented value of the given data in string format.
 *
 * @param[in] i_keywordValue - Data in binary format.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return - Returns the converted string value.
 */
inline std::string getPrintableValue(const types::BinaryVector& i_keywordValue,
                                     uint16_t& o_errCode)
{
    o_errCode = 0;
    std::ostringstream l_oss;
    try
    {
        bool l_allPrintable = std::all_of(
            i_keywordValue.begin(), i_keywordValue.end(),
            [](const auto& l_byte) { return std::isprint(l_byte); });

        if (l_allPrintable)
        {
            l_oss << std::string(i_keywordValue.begin(), i_keywordValue.end());
        }
        else
        {
            l_oss << "0x";
            for (const auto& l_byte : i_keywordValue)
            {
                l_oss << std::setfill('0') << std::setw(2) << std::hex
                      << static_cast<int>(l_byte);
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }

    return l_oss.str();
}

/**
 * @brief API to get data in binary format.
 *
 * This API converts given string value present in hexadecimal or decimal format
 * into array of binary data.
 *
 * @param[in] i_value - Input data.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return - Array of binary data on success, empty vector in case of error.
 */
inline types::BinaryVector convertToBinary(const std::string& i_value,
                                           uint16_t& o_errCode)
{
    o_errCode = 0;
    types::BinaryVector l_binaryValue{};

    if (i_value.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return l_binaryValue;
    }

    try
    {
        if (i_value.substr(0, 2).compare("0x") == constants::STR_CMP_SUCCESS)
        {
            if (i_value.length() % 2 != 0)
            {
                o_errCode = error_code::INVALID_HEXADECIMAL_VALUE_LENGTH;
                return l_binaryValue;
            }

            auto l_value = i_value.substr(2);

            if (l_value.empty())
            {
                o_errCode = error_code::INVALID_HEXADECIMAL_VALUE;
                return l_binaryValue;
            }

            if (l_value.find_first_not_of("0123456789abcdefABCDEF") !=
                std::string::npos)
            {
                o_errCode = error_code::INVALID_HEXADECIMAL_VALUE;
                return l_binaryValue;
            }

            for (size_t l_pos = 0; l_pos < l_value.length(); l_pos += 2)
            {
                uint8_t l_byte = static_cast<uint8_t>(
                    std::stoi(l_value.substr(l_pos, 2), nullptr, 16));
                l_binaryValue.push_back(l_byte);
            }
        }
        else
        {
            l_binaryValue.assign(i_value.begin(), i_value.end());
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Error while trying to convert value [" + i_value +
            "] to binary, error : " + getErrCodeMsg(o_errCode));
    }

    return l_binaryValue;
}

/**
 * @brief API to get current time stamp since Epoch.
 *
 * @return time stamp in seconds.
 */
inline size_t getCurrentTimeSinceEpoch() noexcept
{
    // Use high_resolution_clock for better precision
    const auto l_now = std::chrono::high_resolution_clock::now();
    auto l_durationSinceEpoch = l_now.time_since_epoch();

    auto l_timeStampSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(l_durationSinceEpoch)
            .count();
    return static_cast<size_t>(l_timeStampSeconds);
}

/**
 * @brief API to check is field mode enabled.
 *
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return true, if field mode is enabled. otherwise false.
 */
inline bool isFieldModeEnabled(uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    try
    {
        std::vector<std::string> l_cmdOutput =
            executeCmd("/sbin/fw_printenv fieldmode", o_errCode);

        if (l_cmdOutput.size() > 0)
        {
            toLower(l_cmdOutput[0]);

            // Remove the new line character from the string.
            l_cmdOutput[0].erase(l_cmdOutput[0].length() - 1);
            return l_cmdOutput[0] == "fieldmode=true";
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Failed to check if field mode is enabled, error : " +
            getErrCodeMsg(o_errCode));
    }

    return false;
}

/**
 * @brief API to get VPD collection mode
 *
 * VPD collection mode can be hardware, mixed mode or file mode. This is
 * determined by reading a u-boot variable.
 *
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return Hardware mode, mixed mode or file mode.
 */
inline types::VpdCollectionMode getVpdCollectionMode(
    uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    types::VpdCollectionMode l_result{types::VpdCollectionMode::DEFAULT_MODE};
    try
    {
        std::vector<std::string> l_cmdOutput =
            commonUtility::executeCmd("/sbin/fw_printenv vpdmode", o_errCode);

        if (l_cmdOutput.size() > 0)
        {
            commonUtility::toLower(l_cmdOutput[0]);

            // Remove the new line character from the string.
            l_cmdOutput[0].erase(l_cmdOutput[0].length() - 1);

            if (l_cmdOutput[0] == "vpdmode=hardware")
            {
                l_result = types::VpdCollectionMode::HARDWARE_MODE;
            }
            else if (l_cmdOutput[0] == "vpdmode=mixed")
            {
                l_result = types::VpdCollectionMode::MIXED_MODE;
            }
            else if (l_cmdOutput[0] == "vpdmode=file")
            {
                l_result = types::VpdCollectionMode::FILE_MODE;
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }

    return l_result;
}

/**
 * @brief API to get effective FRU path
 *
 * API to get effective VPD path for a FRU based on the VPD collection mode.
 *
 * @param[in] i_vpdCollectionMode - VPD collection mode.
 * @param[in,out] io_fruPath - Path to the EEPROM file.
 * @param[out] o_errCode - To set error code in case of error.
 *
 */
inline void getEffectiveFruPath(
    const types::VpdCollectionMode& i_vpdCollectionMode,
    std::string& io_fruPath, uint16_t& o_errCode) noexcept
{
    try
    {
        o_errCode = 0;
        if (types::VpdCollectionMode::FILE_MODE == i_vpdCollectionMode)
        {
            io_fruPath.insert(0, constants::fileModeDirectoryPath);
        }

        // For Hardware mode and mixed mode FRU path is considered as EEPROM
        // path. No change is needed.

        // ToDo: Need to handle path for mixed mode, when mixed mode is fully
        // implemented.
    }
    catch (std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Error while getting effective path, reason: " +
            std::string(l_ex.what()));
    }
}

/**
 * @brief API to convert a container to string
 *
 * API to convert a container to string. The container should be iterable.
 *
 * @param[in] i_vector - container which needs to be converted to a string.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return String representation of the container elements, empty string
 * otherwise.
 */
template <class Container>
std::string toString(const Container& i_container) noexcept
{
    std::string l_result;
    try
    {
        if (i_container.empty())
        {
            l_result = "[]";
        }
        else
        {
            l_result = "[";
            std::for_each(i_container.cbegin(), i_container.cend(),
                          [&l_result](const auto& l_element) {
                              constexpr char l_seperator{','};
                              l_result += l_element.string() + l_seperator;
                          });

            l_result = "]";
        }
    }
    catch (const std::exception& l_ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Error while converting container to string, reason: " +
            std::string(l_ex.what()));
    }
    return l_result;
}

/**
 * @brief API to restart a systemd service
 *
 * This API restarts a systemd service
 *
 * @param[in] i_serviceName - Name of the systemd service
 * @param[out] o_errCode - To set error code in case of error
 *
 * @return true if service is restarted, false otherwise
 */
inline bool restartService(const std::string& i_serviceName,
                           uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    bool l_rc{false};
    if (i_serviceName.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
    }
    try
    {
        const auto l_cmd = "systemctl restart " + i_serviceName;

        executeCmd(l_cmd, o_errCode);

        return o_errCode == vpd::constants::VALUE_0;
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Failed to restart service [" + i_serviceName +
            "]. Error: " + std::string(l_ex.what()));
    }

    return l_rc;
}

} // namespace commonUtility
} // namespace vpd
