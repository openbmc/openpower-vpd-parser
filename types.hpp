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
using BIOSAttrValueType = std::variant<int64_t, std::string>;
using PendingBIOSAttrItemType =
    std::pair<std::string, std::tuple<std::string, BIOSAttrValueType>>;
using PendingBIOSAttrsType = std::vector<PendingBIOSAttrItemType>;

using BIOSAttribute = std::string;
using RecordName = std::string;
using KeywordName = std::string;
using Bitmask = uint8_t;
using BIOSAttributeTable =
    std::unordered_map<BIOSAttribute,
                       std::tuple<RecordName, KeywordName, Bitmask>>;

using BiosProperty = std::tuple<
    std::string, bool, std::string, std::string, std::string,
    std::variant<int64_t, std::string>, std::variant<int64_t, std::string>,
    std::vector<std::tuple<std::string, std::variant<int64_t, std::string>>>>;
using BiosBaseTable = std::variant<std::map<std::string, BiosProperty>>;
using BiosBaseTableType = std::map<std::string, BiosBaseTable>;

namespace inventory
{

using Path = std::string;
using Property = std::string;
using Value = std::variant<bool, size_t, int64_t, std::string, Binary>;
using PropertyMap = std::map<Property, Value>;
using kwdVpdValueTypes = std::variant<size_t, Binary>;

using Interface = std::string;
using InterfaceMap = std::map<Interface, PropertyMap>;

using Object = sdbusplus::message::object_path;
using ObjectMap = std::map<Object, InterfaceMap>;

using VPDfilepath = std::string;
using FruIsMotherboard = bool;
using FrusMap =
    std::multimap<Path, std::tuple<VPDfilepath, VPDfilepath, FruIsMotherboard>>;
using LocationCode = std::string;
using LocationCodeMap = std::unordered_multimap<LocationCode, Path>;
using ListOfPaths = std::vector<sdbusplus::message::object_path>;
using NodeNumber = uint16_t;
using KeywordVpdMap = std::unordered_map<std::string, kwdVpdValueTypes>;

using systemType = std::string;
using deviceTree = std::string;
using deviceTreeMap = std::unordered_map<systemType, deviceTree>;
using PelAdditionalData = std::map<std::string, std::string>;
using Keyword = std::string;
using KeywordData = std::string;
using DbusPropertyMap = std::unordered_map<Keyword, KeywordData>;
using PelAdditionalData = std::map<std::string, std::string>;
using Service = std::string;
using MapperResponse =
    std::map<Path, std::map<Service, std::vector<Interface>>>;
using RestoredEeproms = std::tuple<Path, std::string, Keyword, Binary>;
using ReplaceableFrus = std::vector<VPDfilepath>;
using EssentialFrus = std::vector<Path>;

} // namespace inventory

} // namespace vpd
} // namespace openpower