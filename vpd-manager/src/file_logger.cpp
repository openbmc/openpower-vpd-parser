#include "file_logger.hpp"

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

}; // namespace vpd
