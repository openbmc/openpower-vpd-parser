#include "common_utility.hpp"

#include "const.hpp"

#include <iostream>
#include <phosphor-logging/lg2.hpp>

namespace openpower
{
namespace vpd
{
namespace common
{
namespace utility
{
using namespace constants;
using namespace inventory;

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
    catch (const sdbusplus::exception::exception& e)
    {
        lg2::error("D-Bus call exception : {OBJPATH}, {INTERFACE}, {EXCEPTION}",
                   "OBJPATH", mapperObjectPath, "INTERFACE", mapperInterface,
                   "EXCEPTION", e.what());

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

        try
        {
            auto result = bus.call(pimMsg);
        }
        catch (const sdbusplus::exception::exception& ex)
        {
            lg2::error("PIM Notify() failed, what() : {ERROR}", "ERROR",
                       ex.what());
        }
    }
    catch (const std::runtime_error& e)
    {
        lg2::error("callPIM failed due to : {ERROR}", "ERROR", e.what());
    }
}

} // namespace utility
} // namespace common
} // namespace vpd
} // namespace openpower