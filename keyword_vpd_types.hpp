#pragma once

#include <stdint.h>
#include <map>
#include <unordered_map>
#include <vector>

namespace vpd
{
namespace keyword
{
namespace parser
{
using Binary = std::vector<uint8_t>;
using KeywordVpdMap = std::map<std::string, std::vector<uint8_t>>;
} // namespace parser
} // namespace keyword
} // namespace vpd
