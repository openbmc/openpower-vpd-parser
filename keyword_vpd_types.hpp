#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace vpd
{
namespace keyword
{
namespace parser
{
constexpr uint8_t KW_VPD_START_TAG = 0x82;
constexpr uint8_t KW_VPD_END_TAG = 0x78;
constexpr uint8_t KW_VAL_PAIR_START_TAG = 0x84;
constexpr uint8_t ALT_KW_VAL_PAIR_START_TAG = 0x90;
constexpr uint8_t KW_VAL_PAIR_END_TAG = 0x79;
constexpr int TWO_BYTES = 2;
constexpr int IPZ_DATA_START = 11;
constexpr int KW_VPD_DATA_START = 0;

using Binary = std::vector<uint8_t>;
using KeywordVpdMap = std::unordered_map<std::string, std::vector<uint8_t>>;
} // namespace parser
} // namespace keyword
} // namespace vpd
