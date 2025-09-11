#include "file_logger.hpp"

namespace vpd
{
const std::unordered_map<LogLevel, std::string>
    FileLogger::m_logLevelToStringMap{{LogLevel::DEBUG, "Debug"},
                                      {LogLevel::INFO, "Info"},
                                      {LogLevel::WARNING, "Warning"},
                                      {LogLevel::ERROR, "Error"}};

void FileLogger::logMessage([[maybe_unused]] const std::string& i_message,
                            [[maybe_unused]] const LogLevel i_logLevel)
{
    try
    {
        /*TODO:
            - add timestamp to message
            - acquire mutex
            - write message to file
            - release mutex
        */
    }
    catch (const std::exception& l_ex)
    {
        throw;
    }
}

}; // namespace vpd
