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

/**
 * @brief Keyword VPD D-bus object creation and setting it to the inventory
 *
 * @param[kwValMap] - property map with properties and its corresponding value.
 */
void kwVpdDbusObj(KeywordVpdMap kwValMap);

} // namespace parser
} // namespace keyword
} // namespace vpd
