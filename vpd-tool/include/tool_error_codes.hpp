#pragma once

namespace vpd
{
/**
 * @brief Error codes for vpd-tool operations
 *
 * Error codes start from -2 and can be extended for various
 * vpd-tool operations including writeKeyword, readKeyword, etc.
 */
enum class ErrorCode
{
    INVALID_INPUT_PARAMETER = -2,
    RECORD_NOT_PROVIDED = -3,
    KEYWORD_VALUE_NOT_PROVIDED = -4,
    DBUS_CALL_FAILED = -5,
    FILE_SYSTEM_ERROR = -6,
    FILE_NOT_FOUND = -7,
    STANDARD_EXCEPTION = -8,
    JSON_EXCEPTION = -9,
    EEPROM_PATH_NOT_FOUND = -10,
    EMPTY_FILE = -11,
    KEYWORD_NAME_NOT_PROVIDED = -12,
    DBUS_TYPE_MISMATCH = -13,
    NOT_ALLOWED = -14
};
} // namespace vpd
