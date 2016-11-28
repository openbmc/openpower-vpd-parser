#pragma once

#include <string>
#include <store.hpp>

namespace openpower
{
namespace vpd
{
namespace inventory
{

/** @brief API to write parsed VPD to inventory
 *
 *  @param [in] type - FRU type
 *  @param [in] vpdStore - Store object containing parsed VPD
 *  @param [in] path - FRU object path
 */
void write(const std::string& type,
           Store&& vpdStore,
           const std::string& path);

} // inventory
} // namespace vpd
} // namespace openpower
