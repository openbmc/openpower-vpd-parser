#pragma once

#include <stdint.h>

#include <fstream>
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

    // max number of log entries in file
    size_t m_maxEntries{256};

    // map to convert log level to string
    static const std::unordered_map<LogLevel, std::string>
        m_logLevelToStringMap;

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
        // TODO: open the file in append mode
    }

    ~FileLogger()
    {
        // TODO: close the file stream
    }

    /**
     * @brief API to log a message to file
     *
     * @param[in] i_message - Message to log
     * @param[in] i_logLevel - Log level of the message
     *
     * @throw std::runtime_error
     */
    void logMessage(
        [[maybe_unused]] const std::string& i_message,
        [[maybe_unused]] const LogLevel i_logLevel = LogLevel::INFO);
};

}; // namespace vpd
