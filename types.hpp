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
using namespace std::string_literals;

using Path = std::string;
using Property = std::string;
using Value = std::variant<bool, size_t, int64_t, std::string, Binary>;
using kwdVpdValueTypes = std::variant<size_t, Binary>;

using Interface = std::string;
using Object = sdbusplus::message::object_path;
using VPDfilepath = std::string;
using FruIsMotherboard = bool;
using LocationCode = std::string;
using NodeNumber = uint16_t;

using systemType = std::string;
using deviceTree = std::string;
using Keyword = std::string;
using RecordName = std::string;
using KeywordData = std::string;
using Service = std::string;
using BrandType = std::string;

using PropertyMap = std::map<Property, Value>;
using InterfaceMap = std::map<Interface, PropertyMap>;
using ObjectMap = std::map<Object, InterfaceMap>;
using FrusMap = std::multimap<Path, std::pair<VPDfilepath, FruIsMotherboard>>;
using LocationCodeMap = std::unordered_multimap<LocationCode, Path>;
using ListOfPaths = std::vector<sdbusplus::message::object_path>;
using KeywordVpdMap = std::unordered_map<std::string, kwdVpdValueTypes>;
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
using brandRecKwdMap =
    std::map<BrandType, std::map<RecordName, std::vector<Keyword>>>;

} // namespace inventory

} // namespace vpd
} // namespace openpower
