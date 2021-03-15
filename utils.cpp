#include "config.h"

#include "utils.hpp"

#include "defines.hpp"

#include <iomanip>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
#include <sstream>
#include <xyz/openbmc_project/Common/error.hpp>

namespace openpower
{
namespace vpd
{
using namespace openpower::vpd::constants;
using namespace inventory;
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace inventory
{

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface)
{
    auto mapper = bus.new_method_call(mapperDestination, mapperObjectPath,
                                      mapperInterface, "GetObject");
    mapper.append(path, std::vector<std::string>({interface}));

    std::map<std::string, std::vector<std::string>> response;
    try
    {
        auto reply = bus.call(mapper);
        reply.read(response);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("D-Bus call exception",
                        entry("OBJPATH=%s", mapperObjectPath),
                        entry("INTERFACE=%s", mapperInterface),
                        entry("EXCEPTION=%s", e.what()));

        throw std::runtime_error("Service name is not found");
    }

    if (response.empty())
    {
        throw std::runtime_error("Service name response is empty");
    }

    return response.begin()->first;
}

void callPIM(ObjectMap&& objects)
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto service = getService(bus, pimPath, pimIntf);
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
        log<level::ERR>(e.what());
    }
}

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

void createPEL(const std::map<std::string, std::string>& additionalData,
               const std::string& errIntf)
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto service = getService(bus, loggerObjectPath, loggerCreateInterface);
        auto method = bus.new_method_call(service.c_str(), loggerObjectPath,
                                          loggerCreateInterface, "Create");
        method.append(errIntf, "xyz.openbmc_project.Logging.Entry.Level.Error",
                      additionalData);

        auto resp = bus.call(method);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        cerr << e.what() << endl;
        throw std::runtime_error(
            "Error in invoking D-Bus logging create interface to register PEL");
    }
}

const string byteVecToHexString(Binary::const_iterator itrBegin,
                                short unsigned int length)
{
    ostringstream ss;
    ss << hex << uppercase << setfill('0');
    ss << "0x";
    while (length != 0)
    {
        ss << hex << setw(2) << (int)*itrBegin;
        ++itrBegin;
        --length;
    }
    const string str = ss.str();
    return str;
}

const string intToString(int n)
{
    stringstream ss;
    ss << n;
    string s;
    ss >> s;
    return s;
}
} // namespace vpd
} // namespace openpower
