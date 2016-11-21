#pragma once

#include <vector>

namespace openpower
{
namespace vpd
{

/** @brief The OpenPOWER VPD, in binary, is specified as a
 *         sequence of bytes */
using Byte = uint8_t;
using Binary = std::vector<Byte>;

/** @brief Forward-declare class Store */
class Store;

/** @brief API to parse OpenPOWER VPD
 *
 *  @param [in] vpd - OpenPOWER VPD in binary format
 *  @returns A Store object, which contains the parsed VPD
 */
Store& parse(Binary&& vpd);

} // namespace vpd
} // namespace openpower
