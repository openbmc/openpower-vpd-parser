#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>

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
    std::fstream m_fileStream;

    // max number of log entries in file
    size_t m_maxEntries{256};

    // mutex to make file logging multi-thread safe
    std::mutex m_fileMutex;

  public:
    // deleted methods
    FileLogger() = delete;
    FileLogger(const FileLogger&) = delete;
    FileLogger(const FileLogger&&) = delete;
    FileLogger operator=(const FileLogger&) = delete;
    FileLogger operator=(const FileLogger&&) = delete;

    FileLogger(const std::string& i_fileName, const size_t i_maxEntries) :
        m_fileName{i_fileName}, m_maxEntries{i_maxEntries}
    {
        // open the file in append mode
    }

    /**
     * @brief API to log a message to file
     *
     * @param[in] i_message - Message to log
     * @param[in] i_logLevel - Log level of the message
     */
    void logMessage(
        [[maybe_unused]] const std::string& i_message,
        [[maybe_unused]] const LogLevel i_logLevel = LogLevel::INFO) noexcept;
};

}; // namespace vpd
