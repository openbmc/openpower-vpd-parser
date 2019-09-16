#pragma once

#include <stdint.h>

#include <unordered_map>
#include <vector>

namespace vpd
{
namespace keyword
{
namespace parser
{
using binary = std::vector<uint8_t>;
using map = std::unordered_map<std::string, std::vector<uint8_t>>;
} // namespace parser
} // namespace keyword
} // namespace vpd
