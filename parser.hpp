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
 *  @returns A Store object, which provides access to
 *  the parsed VPD
 */
Store parse(Binary&& vpd);

namespace keyword
{
namespace editor
{
/** @brief API to parse VPD to check for header
 *  and process TOC for PT record
 *  @param [in] vpd - VPD in binary format
 *  @param [in] iterator - pointing to start of VPD file
 *  @returns [out] size of the PT record
 **/
std::size_t processHeaderAndTOC(Binary&& vpd, uint16_t& ptOffset);

} // namespace editor
} // namespace keyword

} // namespace vpd
} // namespace openpower
