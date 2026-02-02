#include "logger.hpp"

#include <utility/event_logger_utility.hpp>

#include <regex>
#include <sstream>

namespace vpd
{
std::shared_ptr<Logger> Logger::m_loggerInstance;

void Logger::logMessage(std::string_view i_message,
                        const PlaceHolder& i_placeHolder,
                        const types::PelInfoTuple* i_pelTuple,
                        const std::source_location& i_location) noexcept
{
    std::ostringstream l_log;
    l_log << "FileName: " << i_location.file_name() << ","
          << " Line: " << i_location.line() << " " << i_message;

    try
    {
        if (i_placeHolder == PlaceHolder::COLLECTION)
        {
#ifdef ENABLE_FILE_LOGGING
            if (m_collectionLogger.get() == nullptr)
            {
                initiateVpdCollectionLogging();

                if (m_collectionLogger.get() != nullptr)
                {
                    // Log it to a specific place.
                    m_collectionLogger->logMessage(l_log.str());
                }
                else
                {
                    std::cout << l_log.str() << std::endl;
                }
            }
            else
            {
                m_collectionLogger->logMessage(l_log.str());
            }
#else
            std::cout << l_log.str() << std::endl;
#endif
        }
        else if (i_placeHolder == PlaceHolder::PEL)
        {
            if (i_pelTuple)
            {
                // By default set severity to informational
                types::SeverityType l_severity =
                    types::SeverityType::Informational;

                if (std::get<1>(*i_pelTuple).has_value())
                {
                    l_severity = (std::get<1>(*i_pelTuple)).value();
                }

                EventLogger::createSyncPel(
                    std::get<0>(*i_pelTuple), l_severity,
                    i_location.file_name(), i_location.function_name(),
                    std::get<2>(*i_pelTuple), std::string(i_message),
                    std::get<3>(*i_pelTuple), std::get<4>(*i_pelTuple),
                    std::get<5>(*i_pelTuple), std::get<6>(*i_pelTuple));
                return;
            }
            std::cout << "Pel info tuple required to log PEL for message <" +
                             l_log.str() + ">"
                      << std::endl;
        }
        else if (i_placeHolder == PlaceHolder::VPD_WRITE)
        {
            if (!m_vpdWriteLogger)
            {
                m_vpdWriteLogger.reset(
                    new SyncFileLogger("/var/lib/vpd/vpdWrite.log", 128));
            }
            m_vpdWriteLogger->logMessage(l_log.str());
        }
        else
        {
            // Default case, let it go to journal.
            std::cout << l_log.str() << std::endl;
        }
    }
    catch (const std::exception& l_ex)
    {
        std::cout << "Failed to log message:[" + l_log.str() +
                         "]. Error: " + std::string(l_ex.what())
                  << std::endl;
    }
}

#ifdef ENABLE_FILE_LOGGING
void Logger::initiateVpdCollectionLogging() noexcept
{
    try
    {
        // collection log file directory
        const std::filesystem::path l_collectionLogDirectory{"/var/lib/vpd"};

        std::error_code l_ec;
        if (!std::filesystem::exists(l_collectionLogDirectory, l_ec))
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

        // maximum number of collection log files to maintain
        constexpr auto l_maxCollectionLogFiles{3};

        if (l_collectionLogFileCount >= l_maxCollectionLogFiles)
        {
            // delete oldest collection log file
            l_collectionLogFilePath = l_oldestFilePath;

            logMessage("Deleting collection log file " +
                       l_collectionLogFilePath.string());

            std::error_code l_ec;
            if (!std::filesystem::remove(l_collectionLogFilePath, l_ec))
            {
                logMessage("Failed to delete existing collection log file " +
                           l_collectionLogFilePath.string() +
                           " Error: " + l_ec.message());
            }
        }
        else
        {
            l_collectionLogFilePath +=
                "_" + std::to_string(l_collectionLogFileCount) + ".log";
        }

        // create collection logger object with collection_(n+1).log
        m_collectionLogger.reset(
            new AsyncFileLogger(l_collectionLogFilePath, 4096));
    }
    catch (const std::exception& l_ex)
    {
        logMessage("Failed to initialize collection logger. Error: " +
                   std::string(l_ex.what()));
    }
}
#endif

void SyncFileLogger::logMessage(const std::string_view& i_message)
{
    try
    {
        if (m_currentNumEntries >= m_maxEntries)
        {
            rotateFile();
        }

        std::string l_timeStampedMsg{
            timestamp() + " : " + std::string(i_message)};

        // check size of message and pad/trim as required
        if (l_timeStampedMsg.length() > m_logEntrySize)
        {
            l_timeStampedMsg.resize(m_logEntrySize);
        }
        else if (l_timeStampedMsg.length() < m_logEntrySize)
        {
            constexpr char l_padChar{' '};
            l_timeStampedMsg.append(m_logEntrySize - l_timeStampedMsg.length(),
                                    l_padChar);
        }

        // write the message to file
        m_fileStream << l_timeStampedMsg << std::endl;

        // increment number of entries only if write to file is successful
        ++m_currentNumEntries;
    }
    catch (const std::exception& l_ex)
    {
        // log message to journal if we fail to log to file
        auto l_logger = Logger::getLoggerInstance();
        l_logger->logMessage(i_message);
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
        // log message to journal if we fail to push message to queue
        Logger::getLoggerInstance()->logMessage(i_message);
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
        if (!m_fileStream.is_open() || m_stopLogging)
        {
            break;
        }

        // wait for notification from log producer
        m_cv.wait(l_lock,
                  [this] { return m_stopLogging || !m_messageQueue.empty(); });

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

                // flush the message to file
                m_fileStream << l_logMessage << std::endl;

                // lock mutex on queue
                l_lock.lock();
            }
            catch (const std::exception& l_ex)
            {
                // log message to journal if we fail to push message to queue
                Logger::getLoggerInstance()->logMessage(l_logMessage);

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

ILogFileHandler::ILogFileHandler(const std::filesystem::path& i_filePath,
                                 const size_t i_maxEntries) :
    m_filePath{i_filePath}, m_maxEntries{i_maxEntries}
{
    // check if log file already exists
    std::error_code l_ec;
    const bool l_logFileExists = std::filesystem::exists(m_filePath, l_ec);
    if (l_ec)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to check if log file already exists. Error: " +
            l_ec.message());
    }

    if (!l_logFileExists)
    {
        l_ec.clear();

        // check if the parent directory of the file exists
        if (!std::filesystem::exists(m_filePath.parent_path(), l_ec))
        {
            if (l_ec)
            {
                Logger::getLoggerInstance()->logMessage(
                    "Failed to check if log file parent directory [" +
                    m_filePath.parent_path().string() +
                    "] exists. Error: " + l_ec.message());

                l_ec.clear();
            }

            // create parent directories
            if (!std::filesystem::create_directories(m_filePath.parent_path(),
                                                     l_ec))
            {
                if (l_ec)
                {
                    throw std::runtime_error(
                        "Failed to create parent directory of log file path:[" +
                        m_filePath.string() + "]. Error: " + l_ec.message());
                }
            }
        }
    }

    // enable exception mask to throw on badbit and failbit
    m_fileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);
    // open the file in append mode
    m_fileStream.open(m_filePath, std::ios::out | std::ios::ate);

    if (l_logFileExists)
    {
        // log file already exists, check and update the number of entries
        std::ifstream l_readFileStream{m_filePath};
        for (std::string l_line; std::getline(l_readFileStream, l_line);
             ++m_currentNumEntries)
        {}

        l_readFileStream.close();
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
