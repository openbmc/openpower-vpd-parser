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

namespace keyword
{
namespace editor
{
/** @brief API to parse VPD to check for header
 *  and process TOC for PT record
 *  @param [in] vpd - VPD in binary format
 *  @param [in] iterator - pointing to start of VPD file
 *  @returns [out] size of the PT record file
 **/
std::size_t processHeaderAndTOC(Binary&& vpd, uint16_t& ptOffset);

/**  @brief API to update ECC data
 *   @param[in] - Binary VPD file
 *   @param[in] - iterator to the offset of any record
 *
 *   @return[out] - vpd file with updated ECC
 **/
Binary updateECC(Binary&& vpd, Binary::const_iterator iterator);

} // namespace editor
} // namespace keyword

} // namespace vpd
} // namespace openpower
