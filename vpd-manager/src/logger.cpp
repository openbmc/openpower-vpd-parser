#include "logger.hpp"

#include <sstream>

namespace vpd
{
std::shared_ptr<Logger> Logger::m_loggerInstance;

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
        // Log it to a specific place.
        m_logFileHandler->writeLogToFile(i_placeHolder, l_log.str());
    }
    else if (i_placeHolder == PlaceHolder::VPD_WRITE)
    {
        m_logFileHandler->writeLogToFile(i_placeHolder, l_log.str());
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

void LogFileHandler::writeLogToFile(const PlaceHolder& i_placeHolder,
                                    const std::string& i_msg) noexcept
{
    try
    {
        switch (i_placeHolder)
        {
            case PlaceHolder::COLLECTION:
            {
                if (m_collectionLogger)
                {
                    m_collectionLogger->logMessage(i_msg);
                }
                else
                {
                    auto l_logger = Logger::getLoggerInstance();
                    l_logger->logMessage(i_msg);
                }
                break;
            }
            case PlaceHolder::VPD_WRITE:
            {
                if (m_vpdWriteLogger)
                {
                    m_vpdWriteLogger->logMessage(i_msg);
                }
                else
                {
                    auto l_logger = Logger::getLoggerInstance();
                    l_logger->logMessage(i_msg);
                }
            }
            default:
                break;
        };
    }
    catch (const std::exception& l_ex)
    {
        auto l_logger = Logger::getLoggerInstance();
        l_logger->logMessage("Failed to write log [" + i_msg +
                             "] to file. Error: " + std::string(l_ex.what()));
    }
}

const std::unordered_map<LogLevel, std::string>
    FileLogger::m_logLevelToStringMap{{LogLevel::DEBUG, "Debug"},
                                      {LogLevel::INFO, "Info"},
                                      {LogLevel::WARNING, "Warning"},
                                      {LogLevel::ERROR, "Error"}};

void FileLogger::logMessage(const std::string& i_message,
                            const LogLevel i_logLevel)
{
    try
    {
        // acquire lock on file
        std::lock_guard<std::mutex> l_lock(m_mutex);
        if (++m_currentNumEntries > m_maxEntries)
        {
            // TODO: implement log rotation
            m_currentNumEntries = 0;
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
