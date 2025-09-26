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

void SyncFileLogger::logMessage(const std::string_view& i_message)
{
    try
    {
        if (m_currentNumEntries > m_maxEntries)
        {
            rotateFile();
        }

        m_fileStream << timestamp() << " : " << i_message << std::endl;

        // update current number of entries only if write to file is successful
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

void ILogFileHandler::rotateFile(const unsigned i_numEntriesToDelete)
{
    auto l_logger = Logger::getLoggerInstance();

    // temporary file name
    const auto l_tempFileName{
        m_filePath.stem().string() + "_temp" + m_filePath.extension().string()};

    // open a temporary file
    std::ofstream l_tempFileStream{l_tempFileName,
                                   std::ios::trunc | std::ios::out};

    if (!l_tempFileStream.is_open())
    {
        l_logger->logMessage("Failed to open temp file for rotation");
        return;
    }

    // enable exception mask to throw on badbit and failbit
    l_tempFileStream.exceptions(std::ofstream::badbit | std::ofstream::failbit);

    std::ifstream l_originalLogFileStream{m_filePath};
    if (!l_originalLogFileStream.is_open())
    {
        l_logger->logMessage("Failed to open original log file");
        return;
    }

    // temporary line
    std::string l_line;

    // number of lines copied
    unsigned l_numLinesCopied{0};

    // read the existing file line by line until end of file and write the lines
    // to keep to temporary file
    for (unsigned l_numLinesRead = 0;
         std::getline(l_originalLogFileStream, l_line); ++l_numLinesRead)
    {
        if (l_numLinesRead >= i_numEntriesToDelete)
        {
            // write the line to temporary file
            l_tempFileStream << l_line << std::endl;

            ++l_numLinesCopied;
        }
    }

    // close the original log file
    l_originalLogFileStream.close();

    // close temporary file
    l_tempFileStream.close();

    // close the output stream
    m_fileStream.close();

    // rename temporary file to log file
    std::error_code l_ec;
    std::filesystem::rename(l_tempFileName, m_filePath, l_ec);
    if (l_ec)
    {
        l_logger->logMessage(
            "Failed to rename temporary file to log file. Error: " +
            l_ec.message());
    }

    // re-open the new file
    m_fileStream.open(m_filePath, std::ios::in | std::ios::out | std::ios::app);

    // enable exception mask to throw on badbit and failbit
    m_fileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);

    // update current number of entries
    m_currentNumEntries = l_numLinesCopied;
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

    // open the file in append mode
    m_fileStream.open(m_filePath, std::ios::out | std::ios::app);

    // enable exception mask to throw on badbit and failbit
    m_fileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);

    if (l_logFileExists)
    {
        // log file already exists, check and update the number of entries
        std::ifstream l_readFileStream{m_filePath};

        for (std::string l_line; std::getline(l_readFileStream, l_line);
             ++m_currentNumEntries)
        {}

        l_readFileStream.close();

        // rotate the file if required
        if (m_currentNumEntries > m_maxEntries)
        {
            rotateFile();
        }
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
