#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace vpd
{

// enum for log levels
enum class LogLevel : uint8_t
{
    DEBUG = 1,
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief A class to handle logging messages to file
 *
 * This class implements methods to log messages to a desired file in the
 * filesystem.
 */
class FileLogger
{
    // file name
    std::string m_fileName{};

    // file stream object to do file operations
    std::ofstream m_fileStream;

    // max number of log entries in file
    size_t m_maxEntries{256};

    // current number of log entries in file
    size_t m_currentNumEntries{0};

    // mutex to make file logging multi-thread safe
    std::mutex m_fileMutex;

    // map to convert log level to string
    static const std::unordered_map<LogLevel, std::string>
        m_logLevelToStringMap;

    /**
     * @brief API to generate timestamp in string format
     */
    std::string timestamp() noexcept
    {
        const auto l_now = std::chrono::system_clock::now();
        const auto l_in_time_t = std::chrono::system_clock::to_time_t(l_now);
        const auto l_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              l_now.time_since_epoch()) %
                          1000;

        std::stringstream l_ss;
        l_ss << std::put_time(std::localtime(&l_in_time_t), "%Y-%m-%d %H:%M:%S")
             << "." << std::setfill('0') << std::setw(3) << l_ms.count();
        return l_ss.str();
    }

  public:
    // deleted methods
    FileLogger() = delete;
    FileLogger(const FileLogger&) = delete;
    FileLogger(const FileLogger&&) = delete;
    FileLogger operator=(const FileLogger&) = delete;
    FileLogger operator=(const FileLogger&&) = delete;

    /**
     * @brief Parameterized constructor to initialize a file logger object
     *
     * @param[in] i_fileName - Name of the log file
     * @param[in] i_maxEntries - Maximum number of entries in the log file after
     * which the file will be rotated
     */
    FileLogger(const std::string& i_fileName,
               const size_t i_maxEntries) noexcept :
        m_fileName{i_fileName}, m_maxEntries{i_maxEntries}
    {
        // open the file in append mode
        m_fileStream.open(m_fileName, std::ios::out | std::ios::app);
    }

    ~FileLogger()
    {
        m_fileStream.close();
    }

    /**
     * @brief API to log a message to file
     *
     * @param[in] i_message - Message to log
     * @param[in] i_logLevel - Log level of the message
     *
     * @throw
     */
    void logMessage(const std::string& i_message,
                    const LogLevel i_logLevel = LogLevel::INFO);
};

}; // namespace vpd
