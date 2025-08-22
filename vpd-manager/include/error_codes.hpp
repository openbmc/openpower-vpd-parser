#pragma once

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

// Generic errors.
static constexpr auto INVALID_INPUT_PARAMATER = 3001;
} // namespace error_code
} // namespace vpd
