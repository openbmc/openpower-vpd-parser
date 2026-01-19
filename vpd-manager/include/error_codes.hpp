#pragma once

#include <string>
#include <unordered_map>

namespace vpd
{
enum error_code
{
    // File exceptions
    FILE_NOT_FOUND = 2001, /*Just a random value*/
    FILE_ACCESS_ERROR,
    EMPTY_FILE,
    FILE_SYSTEM_ERROR,

    // JSON exceptions
    INVALID_JSON,
    MISSING_FLAG,
    MISSING_ACTION_TAG,
    FRU_PATH_NOT_FOUND,
    JSON_PARSE_ERROR,
    JSON_MISSING_GPIO_INFO,
    JSON_MISSING_SERVICE_NAME,
    REDUNDANT_PATH_NOT_FOUND,
    ERROR_GETTING_REDUNDANT_PATH,
    NO_EEPROM_PATH,

    // Generic errors.
    INVALID_INPUT_PARAMETER,
    DEVICE_NOT_PRESENT,
    DEVICE_PRESENCE_UNKNOWN,
    GPIO_LINE_EXCEPTION,
    ERROR_PROCESSING_SYSTEM_CMD,
    STANDARD_EXCEPTION,
    DBUS_FAILURE,
    POPEN_FAILED,
    INVALID_HEXADECIMAL_VALUE_LENGTH,
    INVALID_HEXADECIMAL_VALUE,
    INVALID_INVENTORY_PATH,
    SERVICE_RUNNING,
    SERVICE_NOT_RUNNING,

    // VPD specific errors
    UNSUPPORTED_VPD_TYPE,
    KEYWORD_NOT_FOUND,
    OUT_OF_BOUND_EXCEPTION,
    FAILED_TO_DETECT_LOCATION_CODE_TYPE,
    RECEIVED_INVALID_KWD_TYPE_FROM_DBUS,
    INVALID_KEYWORD_LENGTH,
    INVALID_VALUE_READ_FROM_DBUS,
    RECORD_NOT_FOUND
};

const std::unordered_map<int, std::string> errorCodeMap = {
    {error_code::FILE_NOT_FOUND, "File does not exist."},
    {error_code::FILE_ACCESS_ERROR, "Failed to access the file."},
    {error_code::EMPTY_FILE, "Empty file."},
    {error_code::INVALID_JSON,
     "Either JSON is missing FRU tag or invalid JSON object."},
    {error_code::MISSING_FLAG,
     "JSON is missing the flag to procees for the FRU."},
    {error_code::MISSING_ACTION_TAG,
     "JSON is missing the action tag to be performed for the FRU."},
    {error_code::FRU_PATH_NOT_FOUND, "The FRU path is not found in the JSON."},
    {error_code::JSON_PARSE_ERROR, "Error while parsing JSON file."},
    {error_code::INVALID_INPUT_PARAMETER,
     "Either one of the input parameter is invalid or empty."},
    {error_code::JSON_MISSING_GPIO_INFO, "JSON missing required GPIO info."},
    {error_code::JSON_MISSING_SERVICE_NAME,
     "JSON missing the service name for the FRU"},
    {error_code::REDUNDANT_PATH_NOT_FOUND, "No redundant path for the FRU."},
    {error_code::ERROR_GETTING_REDUNDANT_PATH,
     "Error while trying to get redundant path for the FRU"},
    {error_code::NO_EEPROM_PATH, "EEPROM path not found."},
    {error_code::DEVICE_NOT_PRESENT,
     "Presence pin read successfully but device was absent."},
    {error_code::DEVICE_PRESENCE_UNKNOWN, "Exception on presence line GPIO."},
    {error_code::GPIO_LINE_EXCEPTION, "There was an exception in GPIO line."},
    {error_code::ERROR_PROCESSING_SYSTEM_CMD,
     "Error while executing system command tag."},
    {error_code::KEYWORD_NOT_FOUND, "Keyword not found"},
    {error_code::OUT_OF_BOUND_EXCEPTION, "Out of bound error"},
    {error_code::UNSUPPORTED_VPD_TYPE, "This VPD type is not supported"},
    {error_code::STANDARD_EXCEPTION, "Standard Exception thrown"},
    {error_code::FILE_SYSTEM_ERROR, "File system error."},
    {error_code::INVALID_KEYWORD_LENGTH, "Invalid keyword length."},
    {error_code::INVALID_VALUE_READ_FROM_DBUS, "Invalid value read from DBus"},
    {error_code::RECORD_NOT_FOUND, "Record not found."},
    {error_code::FAILED_TO_DETECT_LOCATION_CODE_TYPE,
     "Failed to detect location code type"},
    {error_code::DBUS_FAILURE, "Dbus call failed"},
    {error_code::RECEIVED_INVALID_KWD_TYPE_FROM_DBUS,
     "Received invalid keyword data type from DBus."},
    {error_code::POPEN_FAILED, "popen failed"},
    {error_code::INVALID_HEXADECIMAL_VALUE_LENGTH,
     "Invalid hexadecimal value length."},
    {error_code::INVALID_HEXADECIMAL_VALUE, "Invalid hexadecimal value."},
    {error_code::INVALID_INVENTORY_PATH, "Invalid inventory path."},
    {error_code::SERVICE_RUNNING, "Service is running"},
    {error_code::SERVICE_NOT_RUNNING, "Service is not running"}};
} // namespace vpd
