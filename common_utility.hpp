#pragma once
#include "const.hpp"
#include "types.hpp"

#include <iostream>

namespace openpower
{
namespace vpd
{
namespace common
{
namespace utility
{

/**
 * @brief Check the type of VPD.
 *
 * Checks the type of vpd based on the start tag.
 * @param[in] vector - Vpd data in vector format
 *
 * @return enum of type vpdType
 */
constants::vpdType vpdTypeCheck(const Binary& vector);

/** @brief Api to Get d-bus service for given interface
 *  @param[in] - Bus object
 *  @param[in] - object path of the service
 *  @param[in] - interface under the object path
 *  @return service name
 */
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

/** @brief Call inventory-manager to add objects
 *
 *  @param [in] objects - Map of inventory object paths
 */
void callPIM(inventory::ObjectMap&& objects);

} // namespace utility
} // namespace common
} // namespace vpd
} // namespace openpower