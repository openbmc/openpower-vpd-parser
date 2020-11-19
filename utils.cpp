#include "config.h"

#include "utils.hpp"

#include "defines.hpp"

#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
#include <sstream>

using json = nlohmann::json;

namespace openpower
{
namespace vpd
{
using namespace openpower::vpd::constants;
namespace inventory
{

auto getPIMService()
{
    auto bus = sdbusplus::bus::new_default();
    auto mapper =
        bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetObject");

    mapper.append(pimPath);
    mapper.append(std::vector<std::string>({pimIntf}));

    auto result = bus.call(mapper);
    if (result.is_method_error())
    {
        throw std::runtime_error("ObjectMapper GetObject failed");
    }

    std::map<std::string, std::vector<std::string>> response;
    result.read(response);
    if (response.empty())
    {
        throw std::runtime_error("ObjectMapper GetObject bad response");
    }

    return response.begin()->first;
}

void callPIM(ObjectMap&& objects)
{
    std::string service;

    try
    {
        service = getPIMService();
        auto bus = sdbusplus::bus::new_default();
        auto pimMsg =
            bus.new_method_call(service.c_str(), pimPath, pimIntf, "Notify");
        pimMsg.append(std::move(objects));
        auto result = bus.call(pimMsg);
        if (result.is_method_error())
        {
            std::cerr << "PIM Notify() failed\n";
        }
    }
    catch (const std::runtime_error& e)
    {
        using namespace phosphor::logging;
        log<level::ERR>(e.what());
    }
}

} // namespace inventory

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

const string getIM(const Parsed& vpdMap)
{
    Binary imVal;
    auto property = vpdMap.find("VSBP");
    if (property != vpdMap.end())
    {
        auto kw = (property->second).find("IM");
        if (kw != (property->second).end())
        {
            copy(kw->second.begin(), kw->second.end(),
                 back_inserter(imVal));
        }
    }

    ostringstream oss;
    for (auto& i : imVal)
    {
        oss << setw(2) << setfill('0') << hex << static_cast<int>(i);
    }

    return oss.str();
}

const string getPN(const Parsed& vpdMap)
{
    string pnVal;
    auto prop = vpdMap.find("VINI");
    if (prop != vpdMap.end())
    {
        auto kw = (prop->second).find("PN");
        if (kw != (prop->second).end())
        {
            pnVal = kw->second;
        }
    }

    return pnVal;
}

string getSystemsJson(const Parsed& vpdMap)
{
    ifstream systemJson(SYSTEM_JSON);
    auto js = json::parse(systemJson);

    const string partNumber = getPN(vpdMap);
    const string imKeyword = getIM(vpdMap);

    if (js.find("system") == js.end())
    {
        throw runtime_error("Invalid systems Json");
    }

    if (js["system"].find(imKeyword) == js["system"].end())
    {
        throw runtime_error(
            "Invalid system. The system is not present in the systemsJson");
    }

    string jsonPath = "/usr/share/vpd/";
    string jsonName{};

    string pn{};
    for (auto& systems : js.items())
    {
        for (auto& systemType : systems.value().items())
        {
            if (systemType.key() == imKeyword)
            {
                for (auto& key : systemType.value().items())
                {
                    if (key.value().is_object())
                    {
                        pn = key.value().value("PN", "");
                        if (pn == partNumber)
                        {
                            jsonName = key.value().value("json", "");
                            break;
                        }
                    }
                    else if (key.value().is_string())
                    {
                        jsonName = key.value().get<string>();
                        break;
                    }
                }
            }
        }
    }

    jsonPath += jsonName;

    return jsonPath;
}
} // namespace vpd
} // namespace openpower
