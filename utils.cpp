#include "config.h"

#include "utils.hpp"

#include "defines.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>

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

std::string makeDbusCall(std::string objectPath, std::string operation,
                         std::string parameterType, ...)
{
    std::string propVal{};

    auto bus = sdbusplus::bus::new_default();
    auto properties = bus.new_method_call(pimIntf, objectPath.c_str(), dbusInf,
                                          operation.c_str());

    // need this because last argument should be treated as variant in case of
    // Set operation
    size_t numberOfProperties = parameterType.length();
    size_t itemCounter = 0;

    va_list arguments;
    va_start(arguments, parameterType);
    for (const auto& item : parameterType)
    {
        itemCounter++;
        switch (item)
        {
            // sdbus object type
            case 'o':
                properties.append(
                    va_arg(arguments, sdbusplus::message::object_path));
                break;

            // string type
            case 's':
                // implies we are processing last argument
                if (operation == "Set" && itemCounter == numberOfProperties)
                {
                    std::variant<string> argData =
                        va_arg(arguments, std::string);
                    properties.append(argData);
                }
                else
                {
                    properties.append(va_arg(arguments, std::string));
                }
                break;

            // binary type
            case 'b':
                // implies we are processing last argument
                if (operation == "Set" && itemCounter == numberOfProperties)
                {
                    std::variant<Binary> argData = va_arg(arguments, Binary);
                    properties.append(argData);
                }
                else
                {
                    properties.append(va_arg(arguments, Binary));
                }
                break;
        }
    }
    va_end(arguments);

    auto result = bus.call(properties);
    if (!result.is_method_error())
    {
        // if we have to read some value from BUS
        if (operation == "Get")
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
        else
        {
            propVal = "Success";
        }
    }

    return propVal;
}
} // namespace vpd
} // namespace openpower
