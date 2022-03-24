#pragma once
#include "config.h"

#include "const.hpp"

#include <phosphor-logging/lg2.hpp>

namespace openpower
{
namespace vpd
{
using namespace openpower::vpd::constants;

template <typename T>
T readBusProperty(const string& obj, const string& inf, const string& prop)
{
    T val;
    std::string object = INVENTORY_PATH + obj;
    auto bus = sdbusplus::bus::new_default();
    auto properties = bus.new_method_call(
        pimIntf, object.c_str(), "org.freedesktop.DBus.Properties", "Get");
    properties.append(inf);
    properties.append(prop);

    try
    {
        auto result = bus.call(properties);
        // variant<Binary, string> val;
        result.read(val);
    }
    catch (const sdbusplus::exception::exception& ex)
    {
        lg2::error("readBusProperty() failed, what() : {ERROR}", "ERROR",
                   ex.what());
    }

    return val;
}

} // namespace vpd
} // namespace openpower