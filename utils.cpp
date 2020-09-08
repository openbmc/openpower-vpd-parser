#include "config.h"

#include "utils.hpp"

#include "defines.hpp"

#include <phosphor-logging/log.hpp>
#include <regex>
#include <sdbusplus/server.hpp>

namespace openpower
{
namespace vpd
{

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

using namespace openpower::vpd::constants;
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

string udevToGenericPath(string udevPath)
{
    string file{};
    string i2cPath = "/sys/bus/i2c/drivers/at24/";
    string spiPath = "/sys/bus/spi/drivers/at25/"; // CONFIRM with the path

    if (udevPath.find("i2c") != string::npos) // i2c path
    {
        string i2cBus{};
        regex i2cPattern("((i2c)-[0-9]\\/)+");
        auto i2cWord =
            sregex_iterator(udevPath.begin(), udevPath.end(), i2cPattern);
        for (auto i = i2cWord; i != sregex_iterator(); ++i)
        {
            smatch match = *i;
            i2cBus = match.str();
        }
        regex udevPattern("[^\\s]+" + i2cBus);
        file = std::regex_replace(udevPath, udevPattern, i2cPath);
    }
    else if (udevPath.find("spi") != string::npos) // spi path
    {
        regex spiPattern("((spi)[0-9]\\/)+");
        auto spiWord =
            sregex_iterator(udevPath.begin(), udevPath.end(), spiPattern);
        string spiBus;
        for (auto i = spiWord; i != sregex_iterator(); ++i)
        {
            smatch match = *i;
            spiBus = match.str();
        }
        regex udevPattern("[^\\s]+" + spiBus);
        file = std::regex_replace(udevPath, udevPattern, spiPath);
    }
    else
    {
        throw runtime_error(
            "Udev event generated path is neither i2c nor spi driver's path.");
    }
    return file;
}
} // namespace vpd
} // namespace openpower
