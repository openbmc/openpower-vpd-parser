#pragma once

#include "types.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <source_location>
#include <string_view>

namespace vpd
{

/**
 * @brief Enum class defining placeholder tags.
 *
 * The tag will be used by APIs to identify the endpoint for a given log
 * message.
 */
enum class PlaceHolder
{
    DEFAULT,    /* logs to the journal */
    PEL,        /* Creates a PEL */
    COLLECTION, /* Logs collection messages */
    VPD_WRITE   /* Logs VPD write details */
};

// enum for log levels
enum class LogLevel : uint8_t
{
    DEBUG = 1,
    INFO,
    WARNING,
    ERROR,
    DEFAULT = INFO
};

/**
 * @brief Class to handle file operations w.r.t logging.
 * Based on the placeholder the class will handle different file operations to
 * log error messages.
 */
class LogFileHandler
{
  protected:
    // absolute file path of log file
    std::filesystem::path m_filePath{};

    // file stream object to do file operations
    std::fstream m_fileStream;

    // mutex to make file logging multi-thread safe
    std::mutex m_mutex;

    // max number of log entries in file
    size_t m_maxEntries{256};

    // current number of log entries in file
    size_t m_currentNumEntries{0};

    // map to convert log level to string
    static const std::unordered_map<LogLevel, std::string>
        m_logLevelToStringMap;

    /**
     * @brief API to rotate file.
     *
     * This API rotates the logs within a file by deleting specified number of
     * oldest entries.
     *
     * @param[in] i_numEntriesToDelete - Number of entries to delete.
     *
     * @throw std::runtime_error
     */
    virtual void rotateFile(
        [[maybe_unused]] const unsigned i_numEntriesToDelete = 5);

    /**
     * @brief Constructor.
     * Private so that can't be initialized by class(es) other than friends.
     *
     * @param[in] i_filePath - Absolute path of the log file.
     * @param[in] i_maxEntries - Maximum number of entries in the log file after
     * which the file will be rotated.
     */
    LogFileHandler(const std::filesystem::path& i_filePath,
                   const size_t i_maxEntries) :
        m_filePath{i_filePath}, m_maxEntries{i_maxEntries}
    {
        // TODO: open the file in append mode
    }

    /**
     * @brief API to generate timestamp in string format.
     *
     * @return Returns timestamp in string format on success, otherwise returns
     * empty string in case of any error.
     */
    static std::string timestamp() noexcept
    {
        // TODO: generate timestamp.
        return std::string{};
    }

  public:
    // deleted methods
    LogFileHandler() = delete;
    LogFileHandler(const LogFileHandler&) = delete;
    LogFileHandler(const LogFileHandler&&) = delete;
    LogFileHandler operator=(const LogFileHandler&) = delete;
    LogFileHandler operator=(const LogFileHandler&&) = delete;

    /**
     * @brief API to log a message to file.
     *
     * @param[in] i_message - Message to log.
     * @param[in] i_logLevel - Log level of the message.
     *
     * @throw std::runtime_error
     */
    virtual void logMessage(
        [[maybe_unused]] const std::string_view& i_message,
        [[maybe_unused]] const LogLevel i_logLevel = LogLevel::DEFAULT) = 0;

    // destructor
    virtual ~LogFileHandler()
    {
        // TODO: close the filestream
    }
};

/**
 * @brief Singleton class to handle error logging for the repository.
 */
class Logger
{
  public:
    /**
     * @brief Deleted Methods
     */
    Logger(const Logger&) = delete;  // Copy constructor
    Logger(const Logger&&) = delete; // Move constructor

    /**
     * @brief Method to get instance of Logger class.
     */
    static std::shared_ptr<Logger> getLoggerInstance()
    {
        if (!m_loggerInstance)
        {
            m_loggerInstance = std::shared_ptr<Logger>(new Logger());
        }
        return m_loggerInstance;
    }

    /**
     * @brief API to log a given error message.
     *
     * @param[in] i_message - Message to be logged.
     * @param[in] i_placeHolder - States where the message needs to be logged.
     * Default is journal.
     * @param[in] i_logLevel - Level of the log message.
     * @param[in] i_pelTuple - A structure only required in case message needs
     * to be logged as PEL.
     * @param[in] i_location - Locatuon from where message needs to be logged.
     */
    void logMessage(std::string_view i_message,
                    const PlaceHolder& i_placeHolder = PlaceHolder::DEFAULT,
                    const LogLevel i_logLevel = LogLevel::DEFAULT,
                    const types::PelInfoTuple* i_pelTuple = nullptr,
                    const std::source_location& i_location =
                        std::source_location::current());

    /**
     * @brief API to initiate VPD collection logging.
     *
     * This API initiates VPD collection logging. It checks for existing
     * collection log files and if 3 such files are found, it deletes the oldest
     * file and initiates a VPD collection logger object.
     */
    void initiateVpdCollectionLogging() noexcept;

    /**
     * @brief API to terminate VPD collection logging.
     *
     * This API terminates the VPD collection logging by destroying the
     * associated VPD collection logger object.
     */
    void terminateVpdCollectionLogging()
    {
        // TODO: reset VPD collection logger
    }

  private:
    /**
     * @brief Constructor
     */
    Logger() : m_vpdWriteLogger(nullptr), m_collectionLogger(nullptr)
    {
        // TODO: initiate synchronous logger for VPD write logs
    }

    // Instance to the logger class.
    static std::shared_ptr<Logger> m_loggerInstance;

    // logger object to handle VPD write logs
    std::unique_ptr<LogFileHandler> m_vpdWriteLogger;

    // logger object to handle VPD collection logs
    std::unique_ptr<LogFileHandler> m_collectionLogger;
};

/**
 * @brief The namespace defines logging related methods for VPD.
 * Only for backward compatibility till new logger class comes up.
 */
namespace logging
{

/**
 * @brief An api to log message.
 * This API should be called to log message. It will auto append information
 * like file name, line and function name to the message being logged.
 *
 * @param[in] message - Information that we want  to log.
 * @param[in] location - Object of source_location class.
 */
void logMessage(std::string_view message, const std::source_location& location =
                                              std::source_location::current());
} // namespace logging
} // namespace vpd
