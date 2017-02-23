#pragma once

#include <climits>
#include <vector>
#include <string>
#include <map>
#include <sdbusplus/server.hpp>

namespace openpower
{
namespace vpd
{

/** @brief The OpenPOWER VPD, in binary, is specified as a
 *         sequence of characters */
static_assert((8 == CHAR_BIT), "A byte is not 8 bits!");
using Byte = uint8_t;
using Binary = std::vector<Byte>;

namespace inventory
{

using Path = std::string;
using Property = std::string;
using Value = sdbusplus::message::variant<bool, int64_t, std::string>;
using PropertyMap = std::map<Property, Value>;

using Interface = std::string;
using InterfaceMap = std::map<Interface, PropertyMap>;

using Object = sdbusplus::message::object_path;
using ObjectMap = std::map<Object, InterfaceMap>;

using namespace std::string_literals;
static const auto pimPath = "/xyz/openbmc_project/inventory"s;
static const auto pimIntf = "xyz.openbmc_project.Inventory.Manager"s;

} // namespace inventory

} // namespace vpd
} // namespace openpower
