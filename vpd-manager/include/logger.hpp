#pragma once

#include "file_logger.hpp"
#include "types.hpp"

#include <iostream>
#include <memory>
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
    DEFAULT,   /* logs to the journal */
    PEL,       /* Creates a PEL */
    COLLECTION /* Logs collection messages */
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
     * @param[in] i_msg - Message to log.
     */
    void writeLogToFile(const PlaceHolder& i_placeHolder,
                        const std::string& i_msg) noexcept;

    // Friend class Logger.
    friend class Logger;

  private:
    /**
     * @brief Constructor
     * Private so that can't be initialized by class(es) other than friends.
     */
    LogFileHandler()
    {
        // Create a file rotating logger with 5mb size max and 3 rotated files.
        m_collectionLogger =
            std::make_shared<FileLogger>("/var/lib/vpd/collection.log", 512);
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
