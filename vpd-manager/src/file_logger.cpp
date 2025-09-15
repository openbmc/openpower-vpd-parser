#include "file_logger.hpp"

#include <iostream>
namespace vpd
{

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
        std::lock_guard<std::mutex> l_lock(m_fileMutex);
        if (++m_currentNumEntries > m_maxEntries)
        {
            // delete the first entry in the file
            // move the file pointer to beginning of file
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

void AsyncFileLogger::logMessage(const std::string& i_message,
                                 const LogLevel i_logLevel)
{
    try
    {
        // acquire lock on queue
        std::lock_guard<std::mutex> l_lock(m_fileMutex);
        // push message to queue
        m_messageQueue.emplace(
            timestamp() + "[" + m_logLevelToStringMap.at(i_logLevel) + "]" +
            i_message);
    }
    catch (const std::exception& l_ex)
    {
        throw;
    }
}

void AsyncFileLogger::fileWorker() noexcept
{
    while (m_shouldWorkerThreadRun)
    {
        if (!m_messageQueue.empty())
        {
            // read the first message in queue
            const auto l_logMessage = m_messageQueue.front();
            try
            {
                // pop the message from queue
                m_messageQueue.pop();

                // flush the message to file
                m_fileStream << l_logMessage << std::endl;
            }
            catch (const std::exception& l_ex)
            {
                // redirect log message to journal
                std::cout << l_logMessage << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(m_flushTimeInSecs));
    } // thread loop
}

}; // namespace vpd
