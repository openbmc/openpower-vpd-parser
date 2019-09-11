#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ipz
{
namespace vpd
{
namespace args
{

using Args = std::unordered_map<std::string, std::vector<std::string>>;

/** @brief Command-line argument parser for ipz-read-vpd
 *
 *  @param[in] argc - argument count
 *  @param[in] argv - argument array
 *
 *  @returns map of argument:value
 */
Args parse(int argc, char** argv);

/** @brief Display usage of ipz-vpd-read
 *
 *  @param[in] argv - argument array
 */
void usage(char** argv);

} // namespace args
} // namespace vpd
} // namespace ipz
