#pragma once

#include "constants.hpp"
#include "logger.hpp"

#include <algorithm>
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
        logging::logMessage("popen failed with error " +
                            std::string(strerror(errno)));
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
} // namespace commonUtility
} // namespace vpd
