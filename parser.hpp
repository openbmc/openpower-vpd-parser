#pragma once

#include "store.hpp"

#include <vector>

namespace openpower
{
namespace vpd
{

/** @brief API to parse VPD
 *
 *  @param [in] vpd - VPD in binary format
 *  @returns A Store object, which provides access to
 *           the parsed VPD.
 */
Store parse(Binary&& vpd);

} // namespace vpd
} // namespace openpower
