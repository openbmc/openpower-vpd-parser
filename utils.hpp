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

} // namespace vpd
} // namespace openpower
