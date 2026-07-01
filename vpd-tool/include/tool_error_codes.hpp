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
    EEPROM_PATH_NOT_FOUND = -2,
    KEYWORD_VALUE_NOT_PROVIDED = -3,
    INVALID_IM_VALUE_FORMAT = -4,
    INVALID_IM_VALUE = -5,
    EMPTY_INPUT_PARAMETER = -6,
    WRITE_OPERATION_FAILED = -7,
    HARDWARE_WRITE_FAILED = -8,
    DBUS_WRITE_FAILED = -9,
    BINARY_CONVERSION_FAILED = -10,
    EXCEPTION_OCCURRED = -11,
    UNKNOWN_ERROR = -12
};

} // namespace error_codes
} // namespace vpd
