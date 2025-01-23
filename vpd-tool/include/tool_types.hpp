#pragma once

#include <sdbusplus/message/types.hpp>

#include <cstdint>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

namespace vpd
{
namespace types
{
using BinaryVector = std::vector<uint8_t>;

// This covers mostly all the data type supported over DBus for a property.
// clang-format off
using DbusVariantType = std::variant<
    std::vector<std::tuple<std::string, std::string, std::string>>,
    std::vector<std::string>,
    std::vector<double>,
    std::string,
    int64_t,
    uint64_t,
    double,
    int32_t,
    uint32_t,
    int16_t,
    uint16_t,
    uint8_t,
    bool,
    BinaryVector,
    std::vector<uint32_t>,
    std::vector<uint16_t>,
    sdbusplus::message::object_path,
    std::tuple<uint64_t, std::vector<std::tuple<std::string, std::string, double, uint64_t>>>,
    std::vector<std::tuple<std::string, std::string>>,
    std::vector<std::tuple<uint32_t, std::vector<uint32_t>>>,
    std::vector<std::tuple<uint32_t, size_t>>,
    std::vector<std::tuple<sdbusplus::message::object_path, std::string,
                           std::string, std::string>>
 >;

//IpzType contains tuple of <Record, Keyword>
using IpzType = std::tuple<std::string, std::string>;

//ReadVpdParams either of IPZ or keyword format
using ReadVpdParams = std::variant<IpzType, std::string>;

//KwData contains tuple of <keywordName, KeywordValue>
using KwData = std::tuple<std::string, BinaryVector>;

//IpzData contains tuple of <RecordName, KeywordName, KeywordValue>
using IpzData = std::tuple<std::string, std::string, BinaryVector>;

//WriteVpdParams either of IPZ or keyword format
using WriteVpdParams = std::variant<IpzData, KwData>;
// Return type of ObjectMapper GetObject API
using MapperGetObject = std::map<std::string,std::vector<std::string>>;

// Table row data
using TableRowData = std::vector<std::string>;

// Type used to populate table data
using TableInputData = std::vector<TableRowData>;

// A table column name-size pair
using TableColumnNameSizePair = std::pair<std::string, std::size_t>;

/* Map<Property, Value>*/
using PropertyMap = std::map<std::string, DbusVariantType>;

enum UserOption
{
    Exit,
    UseBackupDataForAll,
    UseSystemBackplaneDataForAll,
    MoreOptions,
    UseBackupDataForCurrent,
    UseSystemBackplaneDataForCurrent,
    NewValueOnBoth,
    SkipCurrent
};

using BiosAttributeCurrentValue =
    std::variant<std::monostate, int64_t, std::string>;
using BiosAttributePendingValue = std::variant<int64_t, std::string>;
using BiosGetAttrRetType = std::tuple<std::string, BiosAttributeCurrentValue,
                                      BiosAttributePendingValue>;

// VPD keyword to BIOS attribute map
struct IpzKeyHash
{
    std::size_t operator()(const IpzType& i_key) const
    {
        return std::hash<std::string>()(std::get<0>(i_key)) ^ (std::hash<std::string>()(std::get<1>(i_key)) << 1);
    }
};

struct IpzKeyEqual
{
    bool operator()(const IpzType& i_leftKey, const IpzType& i_rightKey) const
    {
        return std::get<0>(i_leftKey) == std::get<0>(i_rightKey) && std::get<1>(i_leftKey) == std::get<1>(i_rightKey);
    }
};

// Bios attribute metadata container : {attribute name, number of bits to update in VPD keyword, bit position, enabled value in VPD, disabled value in VPD}
using BiosAttributeMetaData = std::tuple<std::string, uint8_t, uint8_t, uint8_t, uint8_t>;

// IPZ keyword to BIOS attribute map
//{Record, Keyword} -> {attribute name, number of bits to update in VPD keyword, bit
// position, enabled value in VPD, disabled value in VPD}
using BiosAttributeKeywordMap = std::unordered_map<IpzType,std::vector<BiosAttributeMetaData>,IpzKeyHash,IpzKeyEqual>;
} // namespace types
} // namespace vpd
