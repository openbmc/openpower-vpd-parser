#include "logger.hpp"

#include <sstream>

namespace vpd
{
namespace logging
{
void logMessage(std::string_view message, const std::source_location& location)
{
    std::ostringstream log;
    log << "FileName: " << location.file_name() << ","
        << " Line: " << location.line() << " " << message;

    /* TODO: Check on this later.
    log << "FileName: " << location.file_name() << ","
        << " Line: " << location.line() << ","
        << " Func: " << location.function_name() << ", " << message;*/

    std::cout << log.str() << std::endl;
}
} // namespace logging
} // namespace vpd
