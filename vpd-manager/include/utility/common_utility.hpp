#pragma once

#include "config.h"

#include "constants.hpp"
#include "error_codes.hpp"
#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <mutex>
#include <unordered_set>
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
 *                  | Common Utility | - - - - - - -
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
 * @param[in] errCode - error code.
 *
 * @return Error message set for that error code. Otherwise empty
 * string.
 */
inline std::string getErrCodeMsg(const uint16_t& errCode)
{
    if (errorCodeMap.find(errCode) != errorCodeMap.end())
    {
        return errorCodeMap.at(errCode);
    }

    return std::string{};
}

/** @brief Return the hex representation of the incoming byte.
 *
 * @param [in] aByte - The input byte.
 * @returns Null character if input byte is out of bound else returns hex
 * representation of the byte as a character.
 */
constexpr auto toHex(const size_t& aByte) noexcept
{
    constexpr auto map = "0123456789abcdef";

    return (aByte < std::strlen(map)) ? map[aByte] : '\0';
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
inline std::string getCommand(T arg1, Types... args)
{
    std::string cmd = " " + arg1 + getCommand(args...);

    return cmd;
}

// Mutex to wrap popen in multithreaded scenario.
static std::mutex popenMutex;

/**
 * @brief API to create shell command and execute.
 *
 * @throw std::runtime_error.
 *
 * @param[in] arguments for command
 * @param[out] errCode - To set error code in case of error.
 * @returns output of that command
 */
template <typename T, typename... Types>
inline std::vector<std::string> executeCmd(T&& path, uint16_t& errCode,
                                           Types... args)
{
    errCode = 0;
    std::vector<std::string> cmdOutput;
    std::lock_guard<std::mutex> lock(popenMutex);
    try
    {
        std::array<char, constants::CMD_BUFFER_LENGTH> buffer;

        std::string cmd = path + getCommand(args...);

        std::unique_ptr<FILE, decltype(&pclose)> cmdPipe(
            popen(cmd.c_str(), "r"), pclose);

        if (!cmdPipe)
        {
            errCode = error_code::POPEN_FAILED;
            Logger::getLoggerInstance()->logMessage(
                "popen failed with error " +
                std::error_code(errno, std::generic_category()).message());
            return cmdOutput;
        }

        while (fgets(buffer.data(), buffer.size(), cmdPipe.get()) != nullptr)
        {
            cmdOutput.emplace_back(buffer.data());
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Error while trying to execute command [" + std::string(path) +
            "], error : " + std::string(ex.what()));
    }

    return cmdOutput;
}

/** @brief Converts string to lower case.
 *
 * @param [in] str - Input string.
 */
inline void toLower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char character) {
                       return std::tolower(character);
                   });
}

/**
 * @brief An API to get hex representation of the incoming bytes.
 *
 * The API returns the hex represented value of the given input in string format
 * with 0x prefix.
 *
 * @param[in] keywordValue - Vector of input byte.
 *
 * @return - Returns the converted string value.
 */
inline std::string convertByteVectorToHex(
    const types::BinaryVector& keywordValue)
{
    std::ostringstream oss;
    oss << "0x";
    for (const auto& byte : keywordValue)
    {
        oss << std::setfill('0') << std::setw(2) << std::hex
            << static_cast<int>(byte);
    }

    return oss.str();
}

/**
 * @brief An API to convert binary value into ascii/hex representation.
 *
 * If given data contains printable characters, ASCII formatted string value of
 * the input data will be returned. Otherwise if the data has any non-printable
 * value, returns the hex represented value of the given data in string format.
 *
 * @param[in] keywordValue - Data in binary format.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return - Returns the converted string value.
 */
inline std::string getPrintableValue(const types::BinaryVector& keywordValue,
                                     uint16_t& errCode)
{
    errCode = 0;
    std::ostringstream oss;
    try
    {
        bool allPrintable =
            std::all_of(keywordValue.begin(), keywordValue.end(),
                        [](const auto& byte) { return std::isprint(byte); });

        if (allPrintable)
        {
            oss << std::string(keywordValue.begin(), keywordValue.end());
        }
        else
        {
            oss << "0x";
            for (const auto& byte : keywordValue)
            {
                oss << std::setfill('0') << std::setw(2) << std::hex
                    << static_cast<int>(byte);
            }
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }

    return oss.str();
}

/**
 * @brief API to get data in binary format.
 *
 * This API converts given string value present in hexadecimal or decimal format
 * into array of binary data.
 *
 * @param[in] inputVal - Input data.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return - Array of binary data on success, empty vector in case of error.
 */
inline types::BinaryVector convertToBinary(const std::string& inputVal,
                                           uint16_t& errCode)
{
    errCode = 0;
    types::BinaryVector binaryValue{};

    if (inputVal.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return binaryValue;
    }

    try
    {
        if (inputVal.substr(0, 2).compare("0x") == constants::STR_CMP_SUCCESS)
        {
            if (inputVal.length() % 2 != 0)
            {
                errCode = error_code::INVALID_HEXADECIMAL_VALUE_LENGTH;
                return binaryValue;
            }

            auto hexDigits = inputVal.substr(2);

            if (hexDigits.empty())
            {
                errCode = error_code::INVALID_HEXADECIMAL_VALUE;
                return binaryValue;
            }

            if (hexDigits.find_first_not_of("0123456789abcdefABCDEF") !=
                std::string::npos)
            {
                errCode = error_code::INVALID_HEXADECIMAL_VALUE;
                return binaryValue;
            }

            for (size_t pos = 0; pos < hexDigits.length(); pos += 2)
            {
                uint8_t byte = static_cast<uint8_t>(
                    std::stoi(hexDigits.substr(pos, 2), nullptr, 16));
                binaryValue.push_back(byte);
            }
        }
        else
        {
            binaryValue.assign(inputVal.begin(), inputVal.end());
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Error while trying to convert value [" + inputVal +
            "] to binary, error : " + getErrCodeMsg(errCode));
    }

    return binaryValue;
}

/**
 * @brief API to get current time stamp since Epoch.
 *
 * @return time stamp in seconds.
 */
inline size_t getCurrentTimeSinceEpoch() noexcept
{
    // Use high_resolution_clock for better precision
    const auto now = std::chrono::high_resolution_clock::now();
    auto durationSinceEpoch = now.time_since_epoch();

    auto timeStampSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch)
            .count();
    return static_cast<size_t>(timeStampSeconds);
}

/**
 * @brief API to check is field mode enabled.
 *
 * @param[out] errCode - To set error code in case of error.
 *
 * @return true, if field mode is enabled. otherwise false.
 */
inline bool isFieldModeEnabled(uint16_t& errCode) noexcept
{
    errCode = 0;
    try
    {
        std::vector<std::string> cmdOutput =
            executeCmd("/sbin/fw_printenv fieldmode", errCode);

        if (cmdOutput.size() > 0)
        {
            toLower(cmdOutput[0]);

            // Remove the new line character from the string.
            cmdOutput[0].erase(cmdOutput[0].length() - 1);
            return cmdOutput[0] == "fieldmode=true";
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Failed to check if field mode is enabled, error : " +
            getErrCodeMsg(errCode));
    }

    return false;
}

/**
 * @brief API to get VPD collection mode
 *
 * VPD collection mode can be hardware, mixed mode or file mode. This is
 * determined by reading a u-boot variable.
 *
 * @param[out] errCode - To set error code in case of error.
 *
 * @return Hardware mode, mixed mode or file mode.
 */
inline types::VpdCollectionMode getVpdCollectionMode(uint16_t& errCode) noexcept
{
    errCode = 0;
    types::VpdCollectionMode result{types::VpdCollectionMode::DEFAULT_MODE};
    try
    {
        std::vector<std::string> cmdOutput =
            commonUtility::executeCmd("/sbin/fw_printenv vpdmode", errCode);

        if (cmdOutput.size() > 0)
        {
            commonUtility::toLower(cmdOutput[0]);

            // Remove the new line character from the string.
            cmdOutput[0].erase(cmdOutput[0].length() - 1);

            if (cmdOutput[0] == "vpdmode=hardware")
            {
                result = types::VpdCollectionMode::HARDWARE_MODE;
            }
            else if (cmdOutput[0] == "vpdmode=mixed")
            {
                result = types::VpdCollectionMode::MIXED_MODE;
            }
            else if (cmdOutput[0] == "vpdmode=file")
            {
                result = types::VpdCollectionMode::FILE_MODE;
            }
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }

    return result;
}

/**
 * @brief API to get effective FRU path
 *
 * API to get effective VPD path for a FRU based on the VPD collection mode.
 *
 * @param[in] vpdCollectionMode - VPD collection mode.
 * @param[in,out] fruPath - Path to the EEPROM file.
 * @param[out] errCode - To set error code in case of error.
 *
 */
inline void getEffectiveFruPath(
    const types::VpdCollectionMode& vpdCollectionMode, std::string& fruPath,
    uint16_t& errCode) noexcept
{
    try
    {
        errCode = 0;

        // Currently we are just planning to support System VPD in file mode.
        // Support for other FRUs in file mode is still under analysis. Till
        // that point to remove the overhead from caller's end the check is
        // being added in the utility side.
        // ToDo:: Need to update below, if file mode is applies for all EEPROM
        // path's in the system.
        if (types::VpdCollectionMode::FILE_MODE == vpdCollectionMode &&
            fruPath == SYSTEM_VPD_FILE_PATH)
        {
            fruPath.insert(0, constants::fileModeDirectoryPath);
        }

        // For Hardware mode and mixed mode FRU path is considered as EEPROM
        // path. No change is needed.

        // ToDo: Need to handle path for mixed mode, when mixed mode is fully
        // implemented.
    }
    catch (std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Error while getting effective path, reason: " +
            std::string(ex.what()));
    }
}

/**
 * @brief API to restart a systemd service
 *
 * This API restarts a systemd service
 *
 * @param[in] serviceName - Name of the systemd service
 * @param[out] errCode - To set error code in case of error
 *
 * @return true if service is restarted, false otherwise
 */
inline bool restartService(const std::string& serviceName,
                           uint16_t& errCode) noexcept
{
    errCode = 0;
    bool rc{false};
    if (serviceName.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return rc;
    }

    try
    {
        const auto cmd = "systemctl restart " + serviceName;

        executeCmd(cmd, errCode);

        return errCode == vpd::constants::VALUE_0;
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(
            "Failed to restart service [" + serviceName +
            "]. Error: " + std::string(ex.what()));
    }

    return rc;
}

/**
 * @brief API to check if a directory should be skipped during processing
 *
 * This API checks if a given directory entry should be excluded during
 * processing.
 *
 * @param[in] entry - Directory entry to check.
 *
 * @return true if the directory must be skipped, false otherwise.
 *
 * @throw std::filesystem::filesystem_error, std::bad_alloc exceptions.
 */
inline bool skipDirectory(const std::filesystem::directory_entry& entry)
{
#ifdef IBM_SYSTEM
    static const std::unordered_set<std::string> skipDirectoryList{
        "system", "logical_bmc", "ethernet0", "ethernet1"};
#else
    static const std::unordered_set<std::string> skipDirectoryList{};
#endif

    auto leaf = entry.path().filename().string();

    // Convert to lowercase for case-insensitive comparison
    commonUtility::toLower(leaf);

    // Return true if the leaf node is in the skipDirs list
    return skipDirectoryList.find(leaf) != skipDirectoryList.end();
}

/**
 * @brief API to recursively process and delete directories.
 *
 * This API recursively traverses the given directory path and deletes
 * subdirectories that are not in the skip list.
 *
 * @param[in] path - Directory path to process.
 * @param[out] directoryRemoved - Flag indicating if any directory was
 *                                successfully removed.
 * @param[out] errCode - Error code to set incase of exceptions.
 */
inline void deleteDirectory(const std::filesystem::path& path,
                            bool& directoryRemoved, uint16_t& errCode) noexcept
{
    errCode = 0;
    try
    {
        std::error_code ec;
        if (!std::filesystem::is_directory(path, ec))
        {
            if (ec)
            {
                errCode = error_code::FILE_SYSTEM_ERROR;
            }

            return;
        }

        const auto directoryIterator =
            std::filesystem::directory_iterator(path);

        for (const auto& entry : directoryIterator)
        {
            ec.clear();
            if (!std::filesystem::is_directory(entry, ec))
            {
                if (ec)
                {
                    Logger::getLoggerInstance()->logMessage(std::format(
                        "Failed to check if path {} is a directory, error : {}",
                        entry.path().string(), ec.message()));
                }
                continue;
            }

            if (skipDirectory(entry))
            {
                deleteDirectory(entry.path(), directoryRemoved, errCode);

                if (errCode == error_code::FILE_SYSTEM_ERROR)
                {
                    Logger::getLoggerInstance()->logMessage(std::format(
                        "Failed to process directory {}, error : {}.",
                        entry.path().string(), getErrCodeMsg(errCode)));

                    errCode = 0;
                }
                continue;
            }
            std::filesystem::remove_all(entry, ec);

            if (ec)
            {
                Logger::getLoggerInstance()->logMessage(
                    std::format("Failed to delete directory {}, error : {}.",
                                entry.path().string(), ec.message()));

                continue;
            }
            directoryRemoved = true;
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
        Logger::getLoggerInstance()->logMessage(std::format(
            "Exception occured while traversing directory {}, error : {}",
            path.string(), ex.what()));
    }
}
} // namespace commonUtility
} // namespace vpd
