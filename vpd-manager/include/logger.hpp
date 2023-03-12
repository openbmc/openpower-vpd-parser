#pragma once

#include <iostream>
#include <source_location>
#include <string_view>

namespace vpd
{
/**
 * @brief The namespace defines logging related methods for VPD.
 */
namespace logging
{

/**
 * @brief An api to log message.
 * This API should be called to log message. It will auto append information
 * like file name, line and function name to the message being logged.
 *
 * @param[in] message - Information that we want  to log.
 * @param[in] location - Object of source_location class.
 */
void logMessage(std::string_view message, const std::source_location& location =
                                              std::source_location::current());
} // namespace logging
} // namespace vpd
