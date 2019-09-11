#pragma once

#include "store.hpp"

#include <vector>

namespace openpower
{
namespace vpd
{

/** @brief API to parse OpenPOWER VPD
 *
 *  @param [in] vpd - OpenPOWER VPD in binary format
 *  @returns A collection of Store object, which provides access to
 *  the parsed VPD
 */
Stores parse(Binary&& vpd);

} // namespace vpd
} // namespace openpower
