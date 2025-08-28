#pragma once

#include "constants.hpp"
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
/** @brief Return the hex representation of the incoming byte.
 *
 * @param [in] i_aByte - The input byte.
 * @returns Hex representation of the byte as a character.
 */
constexpr auto toHex(size_t i_aByte)
{
    constexpr auto l_map = "0123456789abcdef";
    return l_map[i_aByte];
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
 * @returns output of that command
 */
template <typename T, typename... Types>
inline std::vector<std::string> executeCmd(T&& i_path, Types... i_args)
{
    std::vector<std::string> l_cmdOutput;
    std::array<char, constants::CMD_BUFFER_LENGTH> l_buffer;

    std::string l_cmd = i_path + getCommand(i_args...);

    std::unique_ptr<FILE, decltype(&pclose)> l_cmdPipe(
        popen(l_cmd.c_str(), "r"), pclose);

    if (!l_cmdPipe)
    {
        logging::logMessage(
            "popen failed with error " + std::string(strerror(errno)));
        throw std::runtime_error("popen failed!");
    }
    while (fgets(l_buffer.data(), l_buffer.size(), l_cmdPipe.get()) != nullptr)
    {
        l_cmdOutput.emplace_back(l_buffer.data());
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
 *
 * @throw - Throws std::bad_alloc or std::terminate in case of error.
 *
 * @return - Returns the converted string value.
 */
inline std::string getPrintableValue(const types::BinaryVector& i_keywordValue)
{
    bool l_allPrintable =
        std::all_of(i_keywordValue.begin(), i_keywordValue.end(),
                    [](const auto& l_byte) { return std::isprint(l_byte); });

    std::ostringstream l_oss;
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

    return l_oss.str();
}

/**
 * @brief API to get data in binary format.
 *
 * This API converts given string value present in hexadecimal or decimal format
 * into array of binary data.
 *
 * @param[in] i_value - Input data.
 *
 * @return - Array of binary data on success, throws as exception in case
 * of any error.
 *
 * @throw std::runtime_error, std::out_of_range, std::bad_alloc,
 * std::invalid_argument
 */
inline types::BinaryVector convertToBinary(const std::string& i_value)
{
    if (i_value.empty())
    {
        throw std::runtime_error("Empty input provided");
    }

    types::BinaryVector l_binaryValue{};

    if (i_value.substr(0, 2).compare("0x") == constants::STR_CMP_SUCCESS)
    {
        if (i_value.length() % 2 != 0)
        {
            throw std::runtime_error(
                "Write option accepts 2 digit hex numbers. (Ex. 0x1 "
                "should be given as 0x01).");
        }

        auto l_value = i_value.substr(2);

        if (l_value.empty())
        {
            throw std::runtime_error(
                "Provide a valid hexadecimal input. (Ex. 0x30313233)");
        }

        if (l_value.find_first_not_of("0123456789abcdefABCDEF") !=
            std::string::npos)
        {
            throw std::runtime_error("Provide a valid hexadecimal input.");
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

} // namespace commonUtility
} // namespace vpd
