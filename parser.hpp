#pragma once

#include "const.hpp"
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

namespace keyword
{
namespace editor
{
using namespace openpower::vpd::constants;
/** @brief API to check vpd header
 *  @param [in] vpd - VPDheader in binary format
 */
void processHeader(Binary&& vpd);

} // namespace editor
} // namespace keyword

} // namespace vpd
} // namespace openpower
