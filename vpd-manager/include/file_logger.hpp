#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <queue>
#include <sstream>
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
  protected:
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

        // enable exception mask to throw on badbit and failbit
        m_fileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    }

    virtual ~FileLogger()
    {
        m_fileStream.close();
    }

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

  public:
    // deleted methods
    AsyncFileLogger() = delete;
    AsyncFileLogger(const FileLogger&) = delete;
    AsyncFileLogger(const FileLogger&&) = delete;
    AsyncFileLogger operator=(const FileLogger&) = delete;
    AsyncFileLogger operator=(const FileLogger&&) = delete;

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

    ~AsyncFileLogger()
    {
        stopWorker();
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
};

}; // namespace vpd
