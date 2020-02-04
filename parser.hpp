#pragma once

#include "const.hpp"
#include "store.hpp"

#include <tuple>
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
/** @brief API to parse VPD to check for header
 *  and process TOC for PT record
 *  @param [in] vpd - VPD in binary format
 *  @returns [out] offset and size of PT records
 **/
std::tuple<RecordOffset, std::size_t> processHeaderAndTOC(Binary&& vpd);

} // namespace editor
} // namespace keyword

} // namespace vpd
} // namespace openpower
