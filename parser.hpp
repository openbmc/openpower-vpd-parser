#pragma once

#include "store.hpp"

#include <vector>

namespace openpower
{
namespace vpd
{

/** @brief API to parse  VPD
 *
 *  @param [in] vpd -  VPD in binary format
 *  @returns A collection of Store object and storeBinData object,
 *           which provides access to the parsed VPD.
 *           Store object and storeBinData object will contain
 *           vpd data, where data is in string format and
 *           IPZ vpd data , where data is uint8_t format respectively.
 *
 */
Stores parse(Binary&& vpd);

} // namespace vpd
} // namespace openpower
