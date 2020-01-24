#pragma once

#include "types.hpp"

namespace openpower
{
namespace vpd
{

namespace inventory
{

/** @brief Get inventory-manager's d-bus service
 */
auto getPIMService();

/** @brief Call inventory-manager to add objects
 *
 *  @param [in] objects - Map of inventory object paths
 */
void callPIM(ObjectMap&& objects);

} // namespace inventory

using LE2ByteData = uint16_t;

/**@brief This API reads 2 Bytes of data and swap the read data
 * @param[in] iterator- Pointer pointing to the data to be read
 *
 * @return returns 2 Byte data read at the given pointer
 */
LE2ByteData readUInt16LE(Binary::const_iterator iterator);

} // namespace vpd
} // namespace openpower
