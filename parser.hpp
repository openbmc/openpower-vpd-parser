#pragma once

#include <vector>
#include "store.hpp"

namespace openpower
{
namespace vpd
{

/** @brief API to parse OpenPOWER VPD
 *
 *  @param [in] vpd - OpenPOWER VPD in binary format
 *  @returns A Store object, which provides access to
 *  the parsed VPD
 */
Store parse(Binary&& vpd);

} // namespace vpd
} // namespace openpower
