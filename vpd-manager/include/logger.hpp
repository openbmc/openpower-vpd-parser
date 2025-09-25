#pragma once

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
     */
    void writeLogToFile([[maybe_unused]] const PlaceHolder& i_placeHolder)
    {
        // Handle the file operations.
    }

    // Frined class Logger.
    friend class Logger;

  private:
    /**
     * @brief Constructor
     * Private so that can't be initialized by class(es) other than friends.
     */
    ILogFileHandler() {}

    /* Define APIs to handle file operation as per the placeholder. */
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
                        std::source_location::current());

    /**
     * @brief API to initiate VPD collection logging.
     *
     * This API initiates VPD collection logging. It checks for existing
     * collection log files and if 3 such files are found, it deletes the oldest
     * file and initiates a VPD collection logger object, so that every new VPD
     * collection flow always gets logged into a new file.
     */
    void initiateVpdCollectionLogging() noexcept;

    /**
     * @brief API to terminate VPD collection logging.
     *
     * This API terminates the VPD collection logging by destroying the
     * associated VPD collection logger object.
     */
    void terminateVpdCollectionLogging() noexcept
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
    std::unique_ptr<ILogFileHandler> m_vpdWriteLogger;

    // logger object to handle VPD collection logs
    std::unique_ptr<ILogFileHandler> m_collectionLogger;
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
