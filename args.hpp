#pragma once

#include <string>
#include <unordered_map>

namespace openpower
{
namespace vpd
{
namespace args
{

using Args = std::unordered_map<std::string, std::string>;

/** @brief Command-line argument parser for phosphor-read-op-vpd
 *
 *  @param[in] argc - argument count
 *  @param[in] argv - argument array
 *
 *  @returns map of argument:value
 */
Args parse(int argc, char** argv);

/** @brief Display usage of phosphor-read-op-vpd
 *
 *  @param[in] argv - argument array
 */
void usage(char** argv);

} // namespace args
} // namespace vpd
} // namespace openpower
