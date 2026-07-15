#pragma once

namespace vpd
{
namespace error_codes
{
/**
 * @brief Error codes for vpd-tool operations
 *
 * Error codes start from -2 and can be extended for various
 * vpd-tool operations including writeKeyword, readKeyword, etc.
 */
enum ErrorCode
{
    INVALID_INPUT_PARAMETER = -2,
    RECORD_NOT_PROVIDED = -3,
    KEYWORD_VALUE_NOT_PROVIDED = -4,
    DBUS_CALL_FAILED = -5,
    FILE_SYSTEM_ERROR = -6,
    FILE_NOT_FOUND = -7,
    STANDARD_EXCEPTION = -8,
    JSON_PARSE_ERROR = -9
};

} // namespace error_codes
} // namespace vpd
