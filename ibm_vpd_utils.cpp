#include "config.h"

#include "ibm_vpd_utils.hpp"

#include "common_utility.hpp"
#include "defines.hpp"
#include "vpd_exceptions.hpp"

#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
#include <sstream>
#include <vector>
#include <xyz/openbmc_project/Common/error.hpp>

using json = nlohmann::json;

namespace openpower
{
namespace vpd
{
using namespace openpower::vpd::constants;
using namespace inventory;
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace record;
using namespace openpower::vpd::exceptions;
using namespace common::utility;
using Severity = openpower::vpd::constants::PelSeverity;

// mapping of severity enum to severity interface
static std::unordered_map<Severity, std::string> sevMap = {
    {Severity::INFORMATIONAL,
     "xyz.openbmc_project.Logging.Entry.Level.Informational"},
    {Severity::DEBUG, "xyz.openbmc_project.Logging.Entry.Level.Debug"},
    {Severity::NOTICE, "xyz.openbmc_project.Logging.Entry.Level.Notice"},
    {Severity::WARNING, "xyz.openbmc_project.Logging.Entry.Level.Warning"},
    {Severity::CRITICAL, "xyz.openbmc_project.Logging.Entry.Level.Critical"},
    {Severity::EMERGENCY, "xyz.openbmc_project.Logging.Entry.Level.Emergency"},
    {Severity::ERROR, "xyz.openbmc_project.Logging.Entry.Level.Error"},
    {Severity::ALERT, "xyz.openbmc_project.Logging.Entry.Level.Alert"}};

namespace inventory
{

MapperResponse
    getObjectSubtreeForInterfaces(const std::string& root, const int32_t depth,
                                  const std::vector<std::string>& interfaces)
{
    auto bus = sdbusplus::bus::new_default();
    auto mapperCall = bus.new_method_call(mapperDestination, mapperObjectPath,
                                          mapperInterface, "GetSubTree");
    mapperCall.append(root);
    mapperCall.append(depth);
    mapperCall.append(interfaces);

    MapperResponse result = {};

    try
    {
        auto response = bus.call(mapperCall);

        response.read(result);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Error in mapper GetSubTree",
                        entry("ERROR=%s", e.what()));
    }

    return result;
}

} // namespace inventory

LE2ByteData readUInt16LE(Binary::const_iterator iterator)
{
    LE2ByteData lowByte = *iterator;
    LE2ByteData highByte = *(iterator + 1);
    lowByte |= (highByte << 8);
    return lowByte;
}

/** @brief Encodes a keyword for D-Bus.
 */
string encodeKeyword(const string& kw, const string& encoding)
{
    if (encoding == "MAC")
    {
        string res{};
        size_t first = kw[0];
        res += toHex(first >> 4);
        res += toHex(first & 0x0f);
        for (size_t i = 1; i < kw.size(); ++i)
        {
            res += ":";
            res += toHex(kw[i] >> 4);
            res += toHex(kw[i] & 0x0f);
        }
        return res;
    }
    else if (encoding == "DATE")
    {
        // Date, represent as
        // <year>-<month>-<day> <hour>:<min>
        string res{};
        static constexpr uint8_t skipPrefix = 3;

        auto strItr = kw.begin();
        advance(strItr, skipPrefix);
        for_each(strItr, kw.end(), [&res](size_t c) { res += c; });

        res.insert(BD_YEAR_END, 1, '-');
        res.insert(BD_MONTH_END, 1, '-');
        res.insert(BD_DAY_END, 1, ' ');
        res.insert(BD_HOUR_END, 1, ':');

        return res;
    }
    else // default to string encoding
    {
        return string(kw.begin(), kw.end());
    }
}

string readBusProperty(const string& obj, const string& inf, const string& prop)
{
    std::string propVal{};
    std::string object = INVENTORY_PATH + obj;
    auto bus = sdbusplus::bus::new_default();
    auto properties = bus.new_method_call(
        "xyz.openbmc_project.Inventory.Manager", object.c_str(),
        "org.freedesktop.DBus.Properties", "Get");
    properties.append(inf);
    properties.append(prop);
    auto result = bus.call(properties);
    if (!result.is_method_error())
    {
        variant<Binary, string> val;
        result.read(val);
        if (auto pVal = get_if<Binary>(&val))
        {
            propVal.assign(reinterpret_cast<const char*>(pVal->data()),
                           pVal->size());
        }
        else if (auto pVal = get_if<string>(&val))
        {
            propVal.assign(pVal->data(), pVal->size());
        }
    }
    return propVal;
}

void createPEL(const std::map<std::string, std::string>& additionalData,
               const Severity& sev, const std::string& errIntf)
{
    try
    {
        std::string pelSeverity =
            "xyz.openbmc_project.Logging.Entry.Level.Error";
        auto bus = sdbusplus::bus::new_default();
        auto service = getService(bus, loggerObjectPath, loggerCreateInterface);
        auto method = bus.new_method_call(service.c_str(), loggerObjectPath,
                                          loggerCreateInterface, "Create");

        auto itr = sevMap.find(sev);
        if (itr != sevMap.end())
        {
            pelSeverity = itr->second;
        }

        method.append(errIntf, pelSeverity, additionalData);
        auto resp = bus.call(method);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        throw std::runtime_error(
            "Error in invoking D-Bus logging create interface to register PEL");
    }
}

inventory::VPDfilepath getVpdFilePath(const string& jsonFile,
                                      const std::string& ObjPath)
{
    ifstream inventoryJson(jsonFile);
    const auto& jsonObject = json::parse(inventoryJson);
    inventory::VPDfilepath filePath{};

    if (jsonObject.find("frus") == jsonObject.end())
    {
        throw(VpdJsonException(
            "Invalid JSON structure - frus{} object not found in ", jsonFile));
    }

    const nlohmann::json& groupFRUS =
        jsonObject["frus"].get_ref<const nlohmann::json::object_t&>();
    for (const auto& itemFRUS : groupFRUS.items())
    {
        const std::vector<nlohmann::json>& groupEEPROM =
            itemFRUS.value().get_ref<const nlohmann::json::array_t&>();
        for (const auto& itemEEPROM : groupEEPROM)
        {
            if (itemEEPROM["inventoryPath"]
                    .get_ref<const nlohmann::json::string_t&>() == ObjPath)
            {
                filePath = itemFRUS.key();
                return filePath;
            }
        }
    }

    return filePath;
}

bool isPathInJson(const std::string& eepromPath)
{
    bool present = false;
    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);

    try
    {
        auto js = json::parse(inventoryJson);
        if (js.find("frus") == js.end())
        {
            throw(VpdJsonException(
                "Invalid JSON structure - frus{} object not found in ",
                INVENTORY_JSON_SYM_LINK));
        }
        json fruJson = js["frus"];

        if (fruJson.find(eepromPath) != fruJson.end())
        {
            present = true;
        }
    }
    catch (json::parse_error& ex)
    {
        throw(VpdJsonException("Json Parsing failed", INVENTORY_JSON_SYM_LINK));
    }
    return present;
}

bool isRecKwInDbusJson(const std::string& recordName,
                       const std::string& keyword)
{
    ifstream propertyJson(DBUS_PROP_JSON);
    json dbusProperty;
    bool present = false;

    if (propertyJson.is_open())
    {
        try
        {
            auto dbusPropertyJson = json::parse(propertyJson);
            if (dbusPropertyJson.find("dbusProperties") ==
                dbusPropertyJson.end())
            {
                throw(VpdJsonException("dbusProperties{} object not found in "
                                       "DbusProperties json : ",
                                       DBUS_PROP_JSON));
            }

            dbusProperty = dbusPropertyJson["dbusProperties"];
            if (dbusProperty.contains(recordName))
            {
                const vector<string>& kwdsToPublish = dbusProperty[recordName];
                if (find(kwdsToPublish.begin(), kwdsToPublish.end(), keyword) !=
                    kwdsToPublish.end()) // present
                {
                    present = true;
                }
            }
        }
        catch (json::parse_error& ex)
        {
            throw(VpdJsonException("Json Parsing failed", DBUS_PROP_JSON));
        }
    }
    else
    {
        // If dbus properties json is not available, we assume the given
        // record-keyword is part of dbus-properties json. So setting the bool
        // variable to true.
        present = true;
    }
    return present;
}

vpdType vpdTypeCheck(const Binary& vpdVector)
{
    // Read first 3 Bytes to check the 11S bar code format
    std::string is11SFormat = "";
    for (uint8_t i = 0; i < FORMAT_11S_LEN; i++)
    {
        is11SFormat += vpdVector[MEMORY_VPD_DATA_START + i];
    }

    if (vpdVector[IPZ_DATA_START] == KW_VAL_PAIR_START_TAG)
    {
        // IPZ VPD FORMAT
        return vpdType::IPZ_VPD;
    }
    else if (vpdVector[KW_VPD_DATA_START] == KW_VPD_START_TAG)
    {
        // KEYWORD VPD FORMAT
        return vpdType::KEYWORD_VPD;
    }
    else if (is11SFormat.compare(MEMORY_VPD_START_TAG) == 0)
    {
        // Memory VPD format
        return vpdType::MEMORY_VPD;
    }

    // INVALID VPD FORMAT
    return vpdType::INVALID_VPD_FORMAT;
}

string getCommand()
{
    return "";
}

} // namespace vpd
} // namespace openpower
