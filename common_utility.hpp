#pragma once
#include "types.hpp"

namespace openpower
{
namespace vpd
{
namespace common
{
namespace utility
{

/** @brief Api to Get d-bus service for given interface
 *  @param[in] bus - Bus object
 *  @param[in] path - object path of the service
 *  @param[in] interface - interface under the object path
 *  @return service name
 */
std::string getService(sdbusplus::bus_t& bus, const std::string& path,
                       const std::string& interface);

/** @brief Call inventory-manager to add objects
 *
 *  @param [in] objects - Map of inventory object paths
 */
void callPIM(types::ObjectMap&& objects);

} // namespace utility
} // namespace common
} // namespace vpd
} // namespace openpower