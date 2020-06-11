#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace openpower
{
namespace vpd
{
namespace args
{

using Args = std::unordered_map<std::string, std::vector<std::string>>;

/** @brief Command-line argument parser for openpower-read-vpd
 *
 *  @param[in] argc - argument count
 *  @param[in] argv - argument array
 *
 *  @returns map of argument:value
 */
Args parse(int argc, char** argv);

/** @brief Display usage of openpower-vpd-read
 *
 *  @param[in] argv - argument array
 */
void usage(char** argv);

/** @brief Check if all the required arguments are passed.
 *
 *  @param[in] argument list.
 */
bool isArgumentValid(const args::Args& arguments);

} // namespace args
} // namespace vpd
} // namespace openpower
