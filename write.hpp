#pragma once

#include <store.hpp>

#include <string>

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
void write(const std::string& type, const Store& vpdStore,
           const std::string& path);

} // namespace inventory
} // namespace vpd
} // namespace openpower
