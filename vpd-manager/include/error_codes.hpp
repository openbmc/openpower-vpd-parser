#pragma once

#include <string>
#include <unordered_map>

namespace vpd
{
namespace error_code
{
// File exceptions
static constexpr auto FILE_NOT_FOUND = 1001;
static constexpr auto FILE_ACCESS_ERROR = 1002;
static constexpr auto EMPTY_FILE = 1003;

// JSON exceptions
static constexpr auto INVALID_JSON = 2001;
static constexpr auto MISSING_FLAG = 2002;
static constexpr auto MISSING_ACTION_TAG = 2003;
static constexpr auto FRU_PATH_NOT_FOUND = 2004;
static constexpr auto JSON_PARSE_ERROR = 2005;
static constexpr auto JSON_MISSING_GPIO_INFO = 2006;

// Generic errors.
static constexpr auto INVALID_INPUT_PARAMETER = 3001;
static constexpr auto DEVICE_NOT_PRESENT = 2006;
static constexpr auto DEVICE_PRESENCE_UNKNOWN = 2007;
static constexpr auto GPIO_LINE_EXCEPTION = 2008;

const std::unordered_map<int, std::string> errorCodeMap = {
    {FILE_NOT_FOUND, "File does not exist."},
    {FILE_ACCESS_ERROR, "Failed to access the file."},
    {EMPTY_FILE, "Empty file."},
    {INVALID_JSON, "Either JSON is missing FRU tag or invalid JSON object."},
    {MISSING_FLAG, "JSON is missing the flag to procees for the FRU."},
    {MISSING_ACTION_TAG,
     "JSON is missing the action tag to be performed for the FRU."},
    {FRU_PATH_NOT_FOUND, "The FRU path is not found in the JSON."},
    {JSON_PARSE_ERROR, "Error while parsing JSON file."},
    {INVALID_INPUT_PARAMETER,
     "Either one of the input parameter is invalid or empty."},
    {JSON_MISSING_GPIO_INFO, "JSON missing required GPIO info."},
    {DEVICE_NOT_PRESENT,
     "Presence pin read successfully but device was absent."},
    {DEVICE_PRESENCE_UNKNOWN, "Exception on presence line GPIO."},
    {GPIO_LINE_EXCEPTION, "There was an exception in GPIO line."}};
} // namespace error_code
} // namespace vpd
