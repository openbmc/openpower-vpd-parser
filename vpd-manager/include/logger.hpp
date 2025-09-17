#pragma once

#include "types.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <source_location>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_map>

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
    DEFAULT,   /* logs to the journal */
    PEL,       /* Creates a PEL */
    COLLECTION /* Logs collection messages */
};

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
  protected:
    /**
     * @brief Parameterized constructor to initialize a file logger object
     *
     * This API initializes a file logger object to handle logging to file. The
     * constructor is kept as private so that only friend class objects can
     * initialize this.
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

        // enable exception mask to throw on badbit and failbit
        m_fileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    }

    // file name
    std::string m_fileName{};

    // file stream object to do file operations
    std::ofstream m_fileStream;

    // max number of log entries in file
    size_t m_maxEntries{256};

    // current number of log entries in file
    size_t m_currentNumEntries{0};

    // mutex to make file logging multi-thread safe
    std::mutex m_mutex;

    // map to convert log level to string
    static const std::unordered_map<LogLevel, std::string>
        m_logLevelToStringMap;

    /**
     * @brief API to log a message to file
     *
     * @param[in] i_message - Message to log
     * @param[in] i_logLevel - Log level of the message
     *
     * @throw std::runtime_error
     */
    virtual void logMessage(const std::string& i_message,
                            const LogLevel i_logLevel = LogLevel::INFO);

    /**
     * @brief API to generate timestamp in string format
     *
     * @return Returns timestamp in string format on success, otherwise returns
     * empty string in case of any error
     */
    static std::string timestamp() noexcept
    {
        try
        {
            const auto l_now = std::chrono::system_clock::now();
            const auto l_in_time_t =
                std::chrono::system_clock::to_time_t(l_now);
            const auto l_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    l_now.time_since_epoch()) %
                1000;

            std::stringstream l_ss;
            l_ss << std::put_time(std::localtime(&l_in_time_t),
                                  "%Y-%m-%d %H:%M:%S")
                 << "." << std::setfill('0') << std::setw(3) << l_ms.count();
            return l_ss.str();
        }
        catch (const std::exception& l_ex)
        {
            return std::string{};
        }
    }

  public:
    // deleted methods
    FileLogger() = delete;
    FileLogger(const FileLogger&) = delete;
    FileLogger(const FileLogger&&) = delete;
    FileLogger operator=(const FileLogger&) = delete;
    FileLogger operator=(const FileLogger&&) = delete;

    friend class LogFileHandler;

    virtual ~FileLogger()
    {
        if (m_fileStream.is_open())
        {
            m_fileStream.close();
        }
    }
};

/**
 * @brief A class to handle asynchronous logging of messages to file
 *
 * This class implements methods to log messages asynchronously to a desired
 * file in the filesystem. It uses a queue for buffering the messages from
 * caller. The actual file operations are handled by a worker thread.
 */
class AsyncFileLogger : public FileLogger
{
    // queue for log messages
    std::queue<std::string> m_messageQueue;

    // interval in seconds at which the queue is flushed into log file
    unsigned m_flushTimeInSecs{1};

    // flag which controls if the logger worker thread should be running
    std::atomic_bool m_shouldWorkerThreadRun{true};

    /**
     * @brief Logger worker thread body
     */
    void fileWorker() noexcept;

    /**
     * @brief Parameterized constructor to initialize a file logger object
     *
     * @param[in] i_fileName - Name of the log file
     * @param[in] i_maxEntries - Maximum number of entries in the log file after
     * which the file will be rotated
     */
    AsyncFileLogger(const std::string& i_fileName,
                    const size_t i_maxEntries) noexcept :
        FileLogger(i_fileName, i_maxEntries)
    {
        // start worker thread in detached mode
        std::thread{[this]() { this->fileWorker(); }}.detach();
    }

    /**
     * @brief API to log a message to file
     *
     * @param[in] i_message - Message to log
     * @param[in] i_logLevel - Log level of the message
     *
     * @throw
     */
    virtual void logMessage(
        const std::string& i_message,
        const LogLevel i_logLevel = LogLevel::INFO) override;

    /**
     * @brief API to stop logger worker thread
     *
     */
    void stopWorker() noexcept
    {
        m_shouldWorkerThreadRun = false;
    }

  public:
    friend class LogFileHandler;

    // deleted methods
    AsyncFileLogger() = delete;
    AsyncFileLogger(const FileLogger&) = delete;
    AsyncFileLogger(const FileLogger&&) = delete;
    AsyncFileLogger operator=(const FileLogger&) = delete;
    AsyncFileLogger operator=(const FileLogger&&) = delete;

    ~AsyncFileLogger()
    {
        stopWorker();
        if (m_fileStream.is_open())
        {
            m_fileStream.close();
        }
    }
};

/**
 * @brief Class to handle file operations w.r.t logging.
 * Based on the placeholder the class will handle different file operations to
 * log error messages.
 */
class LogFileHandler
{
    // should hold fd's for files required as per placeholder.
  public:
    /**
     * @brief API exposed to write a log message to a file.
     *
     * The API can be called by logger class in case log message needs to be
     * redirected to a file. The endpoint can be decided based on the
     * placeholder passed to the API.
     *
     * @param[in] i_placeHolder - Information about the endpoint.
     * @param[in] i_message - Message to log.
     */
    void writeLogToFile([[maybe_unused]] const PlaceHolder& i_placeHolder,
                        const std::string& i_message) noexcept;

    // Frined class Logger.
    friend class Logger;

  private:
    /**
     * @brief Constructor
     * Private so that can't be initialized by class(es) other than friends.
     */
    LogFileHandler()
    {
        m_collectionLogger = std::shared_ptr<AsyncFileLogger>(
            new AsyncFileLogger("/var/lib/vpd/collection.log", 512));
    }

    // logger object to handle collection logs
    std::shared_ptr<FileLogger> m_collectionLogger;
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
     * @param[in] i_pelTuple - A structure only required in case message needs
     * to be logged as PEL.
     * @param[in] i_location - Locatuon from where message needs to be logged.
     */
    void logMessage(std::string_view i_message,
                    const PlaceHolder& i_placeHolder = PlaceHolder::DEFAULT,
                    const types::PelInfoTuple* i_pelTuple = nullptr,
                    const std::source_location& i_location =
                        std::source_location::current());

  private:
    /**
     * @brief Constructor
     */
    Logger() : m_logFileHandler(nullptr)
    {
        m_logFileHandler =
            std::shared_ptr<LogFileHandler>(new LogFileHandler());
    }

    // Instance to the logger class.
    static std::shared_ptr<Logger> m_loggerInstance;

    // Instance to LogFileHandler class.
    std::shared_ptr<LogFileHandler> m_logFileHandler;
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
