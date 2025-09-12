#include "logger.hpp"

#include <sstream>

namespace vpd
{
std::shared_ptr<Logger> Logger::m_loggerInstance;
const std::unordered_map<LogLevel, std::string>
    LogFileHandler::m_logLevelToStringMap{{LogLevel::DEBUG, "Debug"},
                                          {LogLevel::INFO, "Info"},
                                          {LogLevel::WARNING, "Warning"},
                                          {LogLevel::ERROR, "Error"}};

void Logger::logMessage(std::string_view i_message,
                        const PlaceHolder& i_placeHolder,
                        const types::PelInfoTuple* i_pelTuple,
                        const std::source_location& i_location)
{
    std::ostringstream l_log;
    l_log << "FileName: " << i_location.file_name() << ","
          << " Line: " << i_location.line() << " " << i_message;

    if (i_placeHolder == PlaceHolder::COLLECTION)
    {
        if (m_collectionLogger)
        {
            m_collectionLogger->logMessage(l_log.str());
        }
        else
        {
            // redirect to journal
            std::cout << l_log.str() << std::endl;
        }
    }
    else if (i_placeHolder == PlaceHolder::VPD_WRITE)
    {
        m_vpdWriteLogger->logMessage(l_log.str());
    }
    else if (i_placeHolder == PlaceHolder::PEL)
    {
        if (i_pelTuple)
        {
            // LOG PEL
            // This should call create PEL API from the event logger.
            return;
        }
        std::cout << "Pel info tuple required to log PEL for message <" +
                         l_log.str() + ">"
                  << std::endl;
    }
    else
    {
        // Default case, let it go to journal.
        std::cout << l_log.str() << std::endl;
    }
}

void Logger::initiateVpdCollectionLogging() noexcept
{
    try
    {
        /* TODO:
            - check /var/lib/vpd for number "collection.*" log file
            - if 3 collection_[0-2].log files are found
                - delete collection_1.log
                - create collection logger object with collection_1.log
           parameter
            - else
                - create collection logger object with collection_(n+1).log
           parameter*/

        m_collectionLogger = std::unique_ptr<LogFileHandler>(
            new AsyncFileLogger("/var/lib/vpd/collection.log", 512));
    }
    catch (const std::exception& l_ex)
    {
        std::cerr << "Failed to initialize collection logger" << std::endl;
    }
}

void FileLogger::logMessage([[maybe_unused]] const std::string_view& i_message,
                            [[maybe_unused]] const LogLevel i_logLevel)
{
    try
    {
        // acquire lock on file
        std::lock_guard<std::mutex> l_lock(m_mutex);
        if (++m_currentNumEntries > m_maxEntries)
        {
            rotateFile();
        }
        m_fileStream << timestamp() << " ["
                     << m_logLevelToStringMap.at(i_logLevel) << "] "
                     << i_message << std::endl;
    }
    catch (const std::exception& l_ex)
    {
        throw;
    }
}

void AsyncFileLogger::logMessage(
    [[maybe_unused]] const std::string_view& i_message,
    [[maybe_unused]] const LogLevel i_logLevel)
{
    try
    {
        /*TODO:
            - add timestamp to message
            - acquire mutex
            - push message to queue
            - release mutex
        */
    }
    catch (const std::exception& l_ex)
    {
        throw;
    }
}

void AsyncFileLogger::fileWorker() noexcept
{
    /*TODO:
        - run a loop
        - acquire mutex
        - pop message from queue
        - release mutex
        - write message to file
        - sleep for configured time
    */
}

void LogFileHandler::rotateFile(
    [[maybe_unused]] const unsigned i_numEntriesToDelete)
{
    /* TODO:
        - delete specified number of oldest entries from beginning of file
        - rewrite file to move existing logs to beginning of file
    */
    m_currentNumEntries = m_maxEntries - i_numEntriesToDelete;
}
namespace logging
{
void logMessage(std::string_view message, const std::source_location& location)
{
    std::ostringstream log;
    log << "FileName: " << location.file_name() << ","
        << " Line: " << location.line() << " " << message;

    std::cout << log.str() << std::endl;
}
} // namespace logging
} // namespace vpd
