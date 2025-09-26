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

    if ((i_placeHolder == PlaceHolder::COLLECTION) && m_collectionLogger)
    {
        // Log it to a specific place.
        m_collectionLogger->logMessage(l_log.str());
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
    else if ((i_placeHolder == PlaceHolder::VPD_WRITE) && m_vpdWriteLogger)
    {
        m_vpdWriteLogger->logMessage(l_log.str());
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
        m_collectionLogger.reset(
            new AsyncFileLogger("/var/lib/vpd/collection.log", 512));
    }
    catch (const std::exception& l_ex)
    {
        logMessage("Failed to initialize collection logger. Error: " +
                   std::string(l_ex.what()));
    }
}

void FileLogger::logMessage(const std::string_view& i_message)
{
    try
    {
        // acquire lock on file
        std::lock_guard<std::mutex> l_lock(m_mutex);
        if (++m_currentNumEntries > m_maxEntries)
        {
            rotateFile();
        }
        m_fileStream << timestamp() << " : " << i_message << std::endl;
    }
    catch (const std::exception& l_ex)
    {
        throw;
    }
}

void AsyncFileLogger::logMessage(const std::string_view& i_message)
{
    try
    {
        // acquire lock on queue
        std::unique_lock<std::mutex> l_lock(m_mutex);

        // push message to queue
        m_messageQueue.emplace(timestamp() + " : " + std::string(i_message));

        // notify log worker thread
        m_cv.notify_one();
    }
    catch (const std::exception& l_ex)
    {
        throw;
    }
}

void AsyncFileLogger::fileWorker() noexcept
{
    // create lock object on mutex
    std::unique_lock<std::mutex> l_lock(m_mutex);

    // infinite loop
    while (true)
    {
        // check for exit conditions
        if (!m_fileStream.is_open() || m_shouldStopLogging)
        {
            break;
        }

        // wait for notification from log producer
        m_cv.wait(l_lock, [this] {
            return m_shouldStopLogging || !m_messageQueue.empty();
        });

        // flush the queue
        while (!m_messageQueue.empty())
        {
            // read the first message in queue
            const auto l_logMessage = m_messageQueue.front();
            try
            {
                // pop the message from queue
                m_messageQueue.pop();

                // unlock mutex on queue
                l_lock.unlock();

                if (++m_currentNumEntries > m_maxEntries)
                {
                    rotateFile();
                }

                // flush the message to file
                m_fileStream << l_logMessage << std::endl;

                // lock mutex on queue
                l_lock.lock();
            }
            catch (const std::exception& l_ex)
            {
                // redirect log to journal
                std::cout << "Failed to log message : " << l_logMessage
                          << " Error: " << l_ex.what() << std::endl;

                // check if we need to reacquire lock before continuing to flush
                // queue
                if (!l_lock.owns_lock())
                {
                    l_lock.lock();
                }
            }
        } // queue flush loop
    } // thread loop
}

void LogFileHandler::rotateFile(const unsigned i_numEntriesToDelete)
{
    constexpr auto l_tempFilePath{"/var/lib/temp.log"};

    // open a temporary file
    std::ofstream l_tempFileStream{l_tempFilePath,
                                   std::ios::trunc | std::ios::out};

    // enable exception mask to throw on badbit and failbit
    l_tempFileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);

    // temporary line
    std::string l_line;

    // read the existing file line by line until end of file
    for (unsigned l_numLinesRead = 0; !std::getline(m_fileStream, l_line);
         ++l_numLinesRead)
    {
        if (l_numLinesRead > i_numEntriesToDelete)
        {
            // write the line to temporary file
            l_tempFileStream << l_line;
        }
    }

    // close existing log file
    m_fileStream.close();

    // close temporary file
    l_tempFileStream.close();

    // delete existing log file
    std::error_code l_ec;
    if (!std::filesystem::remove(m_filePath, l_ec))
    {
        auto l_logger = Logger::getLoggerInstance();
        l_logger->logMessage(
            "Failed to delete existing log file. Error: " + l_ec.message());
        return;
    }

    // rename temporary file to log file
    l_ec.clear();
    std::filesystem::rename(l_tempFilePath, m_filePath, l_ec);
    if (l_ec)
    {
        auto l_logger = Logger::getLoggerInstance();
        l_logger->logMessage(
            "Failed to rename temporary file to log file. Error: " +
            l_ec.message());
        return;
    }

    // re-open the new file
    m_fileStream.open(m_filePath);

    // update current number of entries
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
