#pragma once

#include <climits>
#include <map>
#include <sdbusplus/server.hpp>
#include <string>
#include <unordered_map>
#include <vector>

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
using Value = std::variant<bool, int64_t, std::string, Binary>;
using PropertyMap = std::map<Property, Value>;

using Interface = std::string;
using InterfaceMap = std::map<Interface, PropertyMap>;

using Object = sdbusplus::message::object_path;
using ObjectMap = std::map<Object, InterfaceMap>;

using VPDfilepath = std::string;
using FruIsMotherboard = bool;
using FrusMap =
    std::unordered_map<Path, std::pair<VPDfilepath, FruIsMotherboard>>;
using LocationCode = std::string;
using LocationCodeMap = std::unordered_multimap<LocationCode, Path>;
using ListOfPaths = std::vector<sdbusplus::message::object_path>;
using Node = uint16_t;
using namespace std::string_literals;
constexpr auto pimPath = "/xyz/openbmc_project/inventory";
constexpr auto pimIntf = "xyz.openbmc_project.Inventory.Manager";

} // namespace inventory

} // namespace vpd
} // namespace openpower
