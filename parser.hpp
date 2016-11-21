#pragma once

#include <climits>
#include <vector>

namespace openpower
{
namespace vpd
{

/** @brief The OpenPOWER VPD, in binary, is specified as a
 *         sequence of characters */
static_assert((8 == CHAR_BIT), "A byte is not 8 bits!");
using Byte = char;
using Binary = std::vector<Byte>;

/** @brief Forward-declare class Store */
class Store;

/** @brief API to parse OpenPOWER VPD
 *
 *  @param [in] vpd - OpenPOWER VPD in binary format
 *  @returns A Store object, which provides access to
 *  the parsed VPD
 */
decltype(auto) parse(Binary&& vpd);

} // namespace vpd
} // namespace openpower
