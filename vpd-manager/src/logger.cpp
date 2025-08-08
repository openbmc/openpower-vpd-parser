#include "logger.hpp"

#include <sstream>

namespace vpd
{
std::shared_ptr<Logger> Logger::m_loggerInstance;

void Logger::logMessage(std::string_view message,
                        const PlaceHolder& i_placeHolder,
                        const types::PelInfoTuple* pelTuple,
                        const std::source_location& location)
{
    std::ostringstream log;
    log << "FileName: " << location.file_name() << ","
        << " Line: " << location.line() << " " << message;

    if (i_placeHolder == PlaceHolder::COLLECTION)
    {
        // Log it to a specific place.
        m_logFileHandler->writeLogToFile(i_placeHolder);
    }
    else if (i_placeHolder == PlaceHolder::PEL)
    {
        if (pelTuple)
        {
            // LOG PEL
            // This should call create PEL API from the event logger.
            return;
        }
        std::cout << "Pel info tuple required to log PEL for message <" +
                         log.str() + ">"
                  << std::endl;
    }
    else
    {
        // Default case, let it go to journal.
        std::cout << log.str() << std::endl;
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
