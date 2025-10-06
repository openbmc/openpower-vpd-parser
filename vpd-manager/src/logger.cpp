#include "logger.hpp"

#include <regex>
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
        // collection log file directory
        const std::filesystem::path l_collectionLogDirectory{"/var/lib/vpd"};

        std::error_code l_ec;
        if (!std::filesystem::exists(l_collectionLogDirectory))
        {
            if (l_ec)
            {
                throw std::runtime_error(
                    "File system call to exist failed with error = " +
                    l_ec.message());
            }
            throw std::runtime_error(
                "Directory " + l_collectionLogDirectory.string() +
                " does not exist");
        }

        // base name of collection log file
        std::filesystem::path l_collectionLogFilePath{l_collectionLogDirectory};
        l_collectionLogFilePath /= "collection";

        unsigned l_collectionLogFileCount{0};

        std::filesystem::file_time_type l_oldestFileTime;
        std::filesystem::path l_oldestFilePath{l_collectionLogFilePath};

        // iterate through all entries in the log directory
        for (const auto& l_dirEntry :
             std::filesystem::directory_iterator(l_collectionLogDirectory))
        {
            // check /var/lib/vpd for number "collection.*" log file
            const std::regex l_collectionLogFileRegex{"collection.*\\.log"};

            if (std::filesystem::is_regular_file(l_dirEntry.path()) &&
                std::regex_match(l_dirEntry.path().filename().string(),
                                 l_collectionLogFileRegex))
            {
                // check the write time of this file
                const auto l_fileWriteTime =
                    std::filesystem::last_write_time(l_dirEntry.path());

                // update oldest file path if required
                if (l_fileWriteTime < l_oldestFileTime)
                {
                    l_oldestFileTime = l_fileWriteTime;
                    l_oldestFilePath = l_dirEntry.path();
                }

                l_collectionLogFileCount++;
            }
        }

        std::cout << "_SR num collection log files: "
                  << l_collectionLogFileCount << std::endl;

        // maximum number of collection log files to maintain
        constexpr auto l_maxCollectionLogFiles{3};

        if (l_collectionLogFileCount >= l_maxCollectionLogFiles)
        {
            // delete oldest collection log file
            l_collectionLogFilePath = l_oldestFilePath;

            std::cout << "_SR deleting collection log file "
                      << l_collectionLogFilePath << std::endl;

            std::error_code l_ec;
            if (!std::filesystem::remove(l_collectionLogFilePath, l_ec))
            {
                std::cerr << "Failed to delete existing collection log file "
                          << l_collectionLogFilePath
                          << " Error: " << l_ec.message() << std::endl;
            }
        }
        else
        {
            // create collection logger object with collection_(n+1).log
            l_collectionLogFilePath +=
                "_" + std::to_string(l_collectionLogFileCount) + ".log";
        }

        m_collectionLogger.reset(
            new AsyncFileLogger(l_collectionLogFilePath, 512));
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
