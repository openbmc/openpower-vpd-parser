#pragma once

#include "types.hpp"

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
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

/**
 * @brief Class to handle file operations w.r.t logging.
 * Based on the placeholder the class will handle different file operations to
 * log error messages.
 */
class ILogFileHandler
{
  protected:
    // absolute file path of log file
    std::filesystem::path m_filePath{};

    // max number of log entries in file
    size_t m_maxEntries{256};

    // file stream object to do file operations
    std::fstream m_fileStream;

    // current number of log entries in file
    size_t m_currentNumEntries{0};

    // number of chars in a single log entry
    size_t m_logEntrySize{512};

    /**
     * @brief API to rotate file.
     *
     * This API rotates the logs within a file by repositioning the write file
     * pointer to beginning of file. Rotation is achieved by overwriting the
     * oldest log entries starting from the top of the file.
     *
     * @throw std::ios_base::failure
     */
    virtual inline void rotateFile()
    {
        // reset file pointer to beginning of file
        m_fileStream.seekp(0);
        m_currentNumEntries = 0;
    }

    /**
     * @brief Constructor.
     * Private so that can't be initialized by class(es) other than friends.
     *
     * @param[in] i_filePath - Absolute path of the log file.
     * @param[in] i_maxEntries - Maximum number of entries in the log file after
     * which the file will be rotated.
     */
    ILogFileHandler(const std::filesystem::path& i_filePath,
                    const size_t i_maxEntries);

    /**
     * @brief API to generate timestamp in string format.
     *
     * @return Returns timestamp in string format on success, otherwise returns
     * empty string in case of any error.
     */
    static inline std::string timestamp() noexcept
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
    ILogFileHandler() = delete;
    ILogFileHandler(const ILogFileHandler&) = delete;
    ILogFileHandler(const ILogFileHandler&&) = delete;
    ILogFileHandler operator=(const ILogFileHandler&) = delete;
    ILogFileHandler operator=(const ILogFileHandler&&) = delete;

    /**
     * @brief API to log a message to file.
     *
     * @param[in] i_message - Message to log.
     *
     * @throw std::runtime_error
     */
    virtual void logMessage(
        [[maybe_unused]] const std::string_view& i_message) = 0;

    // destructor
    virtual ~ILogFileHandler()
    {
        if (m_fileStream.is_open())
        {
            m_fileStream.close();
        }
    }
};

/**
 * @brief A class to handle logging messages to file synchronously
 *
 * This class handles logging messages to a specific file in a synchronous
 * manner.
 * Note: The logMessage API of this class is not multi-thread safe.
 */
class SyncFileLogger final : public ILogFileHandler
{
    /**
     * @brief Parameterized constructor.
     * Private so that can't be initialized by class(es) other than friends.
     *
     * @param[in] i_filePath - Absolute path of the log file.
     * @param[in] i_maxEntries - Maximum number of entries in the log file after
     * which the file will be rotated.
     */
    SyncFileLogger(const std::filesystem::path& i_filePath,
                   const size_t i_maxEntries) :
        ILogFileHandler(i_filePath, i_maxEntries)
    {}

  public:
    // Friend class Logger.
    friend class Logger;

    // deleted methods
    SyncFileLogger() = delete;
    SyncFileLogger(const SyncFileLogger&) = delete;
    SyncFileLogger(const SyncFileLogger&&) = delete;
    SyncFileLogger operator=(const SyncFileLogger&) = delete;
    SyncFileLogger operator=(const SyncFileLogger&&) = delete;

    /**
     * @brief API to log a message to file
     *
     * This API logs messages to file in a synchronous manner.
     * Note: This API is not multi-thread safe.
     *
     * @param[in] i_message - Message to log
     *
     * @throw std::runtime_error
     */
    void logMessage(const std::string_view& i_message) override;

    // destructor
    ~SyncFileLogger() = default;
};

/**
 * @brief A class to handle asynchronous logging of messages to file
 *
 * This class implements methods to log messages asynchronously to a desired
 * file in the filesystem. It uses a queue for buffering the messages from
 * caller. The actual file operations are handled by a worker thread.
 */
class AsyncFileLogger final : public ILogFileHandler
{
    // queue for log messages
    std::queue<std::string> m_messageQueue;

    // mutex to control access to log message queue
    std::mutex m_mutex;

    // flag which indicates log worker thread if logging is finished
    std::atomic_bool m_stopLogging{false};

    // conditional variable to signal log worker thread
    std::condition_variable m_cv;

    /**
     * @brief Constructor
     * Private so that can't be initialized by class(es) other than friends.
     *
     * @param[in] i_fileName - Name of the log file
     * @param[in] i_maxEntries - Maximum number of entries in the log file after
     * which the file will be rotated
     */
    AsyncFileLogger(const std::filesystem::path& i_fileName,
                    const size_t i_maxEntries) :
        ILogFileHandler(i_fileName, i_maxEntries)
    {
        // start worker thread in detached mode
        std::thread{[this]() { this->fileWorker(); }}.detach();
    }

    /**
     * @brief Logger worker thread body
     */
    void fileWorker() noexcept;

  public:
    // Friend class Logger.
    friend class Logger;

    // deleted methods
    AsyncFileLogger() = delete;
    AsyncFileLogger(const AsyncFileLogger&) = delete;
    AsyncFileLogger(const AsyncFileLogger&&) = delete;
    AsyncFileLogger operator=(const AsyncFileLogger&) = delete;
    AsyncFileLogger operator=(const AsyncFileLogger&&) = delete;

    /**
     * @brief API to log a message to file
     *
     * This API logs given message to a file. This API is multi-thread safe.
     *
     * @param[in] i_message - Message to log
     *
     * @throw std::runtime_error
     */
    void logMessage(const std::string_view& i_message) override;

    // destructor
    ~AsyncFileLogger()
    {
        std::unique_lock<std::mutex> l_lock(m_mutex);

        m_stopLogging = true;

        if (m_fileStream.is_open())
        {
            m_fileStream.close();
        }

        m_cv.notify_one();
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
    Logger(const Logger&) = delete;            // Copy constructor
    Logger(const Logger&&) = delete;           // Move constructor
    Logger operator=(const Logger&) = delete;  // Copy assignment operator
    Logger operator=(const Logger&&) = delete; // Move assignment operator

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
                        std::source_location::current()) noexcept;

#ifdef ENABLE_FILE_LOGGING
    /**
     * @brief API to terminate VPD collection logging.
     *
     * This API terminates the VPD collection logging by destroying the
     * associated VPD collection logger object.
     */
    void terminateVpdCollectionLogging() noexcept
    {
        m_collectionLogger.reset();
    }
#endif

  private:
    /**
     * @brief Constructor
     */
    Logger() : m_vpdWriteLogger(nullptr)
    {
#ifdef ENABLE_FILE_LOGGING
        m_collectionLogger = nullptr;
#endif
    }

#ifdef ENABLE_FILE_LOGGING
    /**
     * @brief API to initiate VPD collection logging.
     *
     * This API initiates VPD collection logging. It checks for existing
     * collection log files and if 3 such files are found, it deletes the oldest
     * file and initiates a VPD collection logger object, so that every new VPD
     * collection flow always gets logged into a new file.
     */
    void initiateVpdCollectionLogging() noexcept;

    // logger object to handle VPD collection logs
    std::unique_ptr<ILogFileHandler> m_collectionLogger;

#endif

    // Instance to the logger class.
    static std::shared_ptr<Logger> m_loggerInstance;

    // logger object to handle VPD write logs
    std::unique_ptr<ILogFileHandler> m_vpdWriteLogger;
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
