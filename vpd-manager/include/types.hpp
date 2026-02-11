#pragma once

#include <phosphor-logging/elog-errors.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/Progress/common.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <tuple>
#include <unordered_map>
#include <variant>

namespace vpd
{
namespace types
{

using BiosProperty = std::tuple<
    std::string, bool, std::string, std::string, std::string,
    std::variant<int64_t, std::string>, std::variant<int64_t, std::string>,
    std::vector<std::tuple<std::string, std::variant<int64_t, std::string>,
                           std::string>>>;
using BiosBaseTable =
    std::variant<std::monostate, std::map<std::string, BiosProperty>>;
using BiosBaseTableType = std::map<std::string, BiosBaseTable>;
using BiosAttributeCurrentValue =
    std::variant<std::monostate, int64_t, std::string>;
using BiosAttributePendingValue = std::variant<int64_t, std::string>;
using BiosGetAttrRetType = std::tuple<std::string, BiosAttributeCurrentValue,
                                      BiosAttributePendingValue>;
using PendingBIOSAttrItem =
    std::pair<std::string, std::tuple<std::string, BiosAttributePendingValue>>;
using PendingBIOSAttrs = std::vector<PendingBIOSAttrItem>;

using BinaryVector = std::vector<uint8_t>;

// This covers mostly all the data type supported over Dbus for a property.
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
                           std::string, std::string>>,
    PendingBIOSAttrs
 >;

using MapperGetObject =
    std::vector<std::pair<std::string, std::vector<std::string>>>;
using MapperGetSubTree = std::map<std::string, std::map<std::string, std::vector<std::string>>>;

/* A type for holding the innermost map of property::value.*/
using IPZKwdValueMap = std::unordered_map<std::string, std::string>;
/*IPZ VPD Map of format <Record name, <keyword, value>>*/
using IPZVpdMap = std::unordered_map<std::string, IPZKwdValueMap>;

/*Value types supported by Keyword VPD*/
using KWdVPDValueType = std::variant<BinaryVector,std::string, size_t>;
/* This hold map of parsed data of keyword VPD type*/
using KeywordVpdMap = std::unordered_map<std::string, KWdVPDValueType>;

/**
 * Both Keyword VPD parser and DDIMM parser stores the
 * parsed VPD in the same format.
 * To have better readability, two types are defined for underneath data structure.
*/
using DdimmVpdMap = KeywordVpdMap;

/**
 * Both Keyword VPD parser and ISDIMM parser stores the
 * parsed SPD in the same format.
*/
using JedecSpdMap = KeywordVpdMap;

/**
 * Type to hold keyword::value map of a VPD.
 * Variant can be extended to support additional type.
*/
using VPDKWdValueMap = std::variant<IPZKwdValueMap, KeywordVpdMap>;

/* Map<Property, Value>*/
using PropertyMap = std::map<std::string, DbusVariantType>;
/* Map<Interface<Property, Value>>*/
using InterfaceMap = std::map<std::string, PropertyMap>;
using ObjectMap = std::map<sdbusplus::message::object_path, InterfaceMap>;

using KwSize = uint8_t;
using RecordId = uint8_t;
using RecordSize = uint16_t;
using RecordType = uint16_t;
using RecordOffset = uint16_t;
using RecordLength = uint16_t;
using ECCOffset = uint16_t;
using ECCLength = uint16_t;
using PoundKwSize = uint16_t;

using RecordOffsetList = std::vector<uint32_t>;

using VPDMapVariant = std::variant<std::monostate, IPZVpdMap, KeywordVpdMap>;

using HWVerList = std::vector<std::pair<std::string, std::string>>;
/**
 * Map of <systemIM, pair<Default version, vector<HW version, JSON suffix>>>
*/
using SystemTypeMap =
    std::unordered_map<std::string, std::pair<std::string, HWVerList>>;

using Path = std::string;
using Record = std::string;
using Keyword = std::string;

using IpzData = std::tuple<Record, Keyword, BinaryVector>;
using KwData = std::tuple<Keyword, BinaryVector>;
using VpdData = std::variant<IpzData, KwData>;

using IpzType = std::tuple<Record, Keyword>;
using ReadVpdParams = std::variant<IpzType, Keyword>;
using WriteVpdParams = std::variant<IpzData, KwData>;

using ListOfPaths = std::vector<sdbusplus::message::object_path>;
using RecordData = std::tuple<RecordOffset, RecordLength, ECCOffset, ECCLength>;

using DbusInvalidArgument =
    sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
using DbusNotAllowed = sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;

using InvalidArgument = phosphor::logging::xyz::openbmc_project::Common::InvalidArgument;

namespace DeviceError = sdbusplus::xyz::openbmc_project::Common::Device::Error;

/* PEL Severity enum as defined in [xyz.openbmc_project.Logging.Entry.Level]log.hpp from 'phosphor-logging' repo. */
enum SeverityType
{
    Notice,
    Informational,
    Debug,
    Warning,
    Critical,
    Emergency,
    Alert,
    Error
};

/* PEL callout priority from 'phosphor-logging' pel_types.hpp. If any change in 'phosphor-logging', it needs update here as well. */
enum CalloutPriority
{
    High,
    Medium,
    MediumGroupA,
    MediumGroupB,
    MediumGroupC,
    Low
};

/* The Message property of the event entry for creating PEL, to introduce new message needs to be added in 'phosphor-logging' message_registry.json as well. */
enum ErrorType
{
    DefaultValue,
    InvalidVpdMessage,
    VpdMismatch,
    InvalidEeprom,
    EccCheckFailed,
    JsonFailure,
    DbusFailure,
    InvalidSystem,
    EssentialFru,
    GpioError,
    InternalFailure, /* Should be used for any generic firmware failure */
    FruMissing, /* Should be used in case of presence failure */
    SystemTypeMismatch,
    UndefinedError,
    UnknownSystemSettings,
    FirmwareError,
    VpdParseError /* Should be used in case of any generic VPD parsing error. */
};

/**
 * @brief Enum which defines VPD collection modes
 */
enum class VpdCollectionMode : uint8_t
{
    HARDWARE_MODE,
    MIXED_MODE,
    FILE_MODE,
    DEFAULT_MODE = HARDWARE_MODE
};

using InventoryCalloutData = std::tuple<std::string, CalloutPriority>;
using DeviceCalloutData = std::tuple<std::string, std::string>;
using I2cBusCalloutData = std::tuple<std::string, std::string, std::string>;

using ExceptionInfoVariant = std::variant<std::monostate, ErrorType, std::string>;
/* Error info map of format <Error format, Value> */
using ExceptionDataMap = std::map<std::string, ExceptionInfoVariant>;

/* Pair of invalid record name and error encountered while parsing the record*/
using InvalidRecordEntry = std::pair<Record,ErrorType>;
/* List of invalid record entries*/
using InvalidRecordList = std::vector<InvalidRecordEntry>;
/* Map of inventory path -> Present property match object */
using FruPresenceMatchObjectMap = std::map<Path, std::shared_ptr<sdbusplus::bus::match_t>>;
/* A map of interface to match object*/
using MatchObjectInterfaceMap = std::map<std::string,std::shared_ptr<sdbusplus::bus::match_t>>;
/* A map of service name to match object interface map*/
using MatchObjectMap = std::map<std::string,MatchObjectInterfaceMap>;

/*
* Tuple of Error type, severity, internal rc, userdata1, userdata2, symFru, Procedure
*/
using PelInfoTuple =
    std::tuple<types::ErrorType, std::optional<types::SeverityType>, uint8_t, std::optional<std::string>,
               std::optional<std::string>, std::optional<std::string>,
               std::optional<std::string>>;
/* A tuple of Dbus object path, interface and property*/
using DbusPropertyEntry = std::tuple<std::string, std::string, std::string>;
/* A list of Dbus property entries */
using DbusPropertyList = std::vector<DbusPropertyEntry>;

using CommonProgress = sdbusplus::common::xyz::openbmc_project::common::Progress;
using VpdCollectionStatus = CommonProgress::OperationStatus;

/* A tuple of EEPROM and Dbus object path*/
using EepromInventoryPaths = std::tuple<std::string, std::string>;
using BinaryStringKwValuePair = std::tuple<types::BinaryVector, std::string>;

/* Tuple of<source record name, source keyword name, destination record name, destination keyword
     * name, default value> */
using SrcDstRecordDetails = std::tuple<std::string&, std::string&, std::string&, std::string&,
                   types::BinaryVector&>;

/* Maps a VPD record name to a list of its keyword names. */
using RecordKeywordsMap = std::unordered_map<types::Record, std::vector<types::Keyword>>;

/* A variant type that holds details of VPD mismatches between the primary EEPROM and its redundant counterpart. */
using MismatchedVpdVariant = std::variant<std::monostate, RecordKeywordsMap>;
} // namespace types
} // namespace vpd
