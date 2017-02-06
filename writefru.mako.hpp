## This file is a template.  The comment below is emitted
## into the rendered file; feel free to edit this file.
// WARNING: Generated header. Do not edit!


#pragma once

#include <map>
#include <iostream>
#include <sdbusplus/server.hpp>
#include <log.hpp>
#include "defines.hpp"
#include "store.hpp"

namespace openpower
{
namespace vpd
{
namespace inventory
{

using Property = std::string;
using Value = sdbusplus::message::variant<bool, int64_t, std::string>;
using PropertyMap = std::map<Property, Value>;

using Interface = std::string;
using InterfaceMap = std::map<Interface, PropertyMap>;

using Object = sdbusplus::message::object_path;
using ObjectMap = std::map<Object, InterfaceMap>;

using namespace std::string_literals;
static const auto pimPath = "/xyz/openbmc_project/Inventory"s;
static const auto pimIntf = "xyz.openbmc_project.Inventory.Manager"s;

/** @brief Get inventory-manager's d-bus service
 */
auto getPIMService()
{
    auto bus = sdbusplus::bus::new_default();
    auto mapper =
        bus.new_method_call(
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/ObjectMapper",
            "xyz.openbmc_project.ObjectMapper",
            "GetObject");

    mapper.append(pimPath);
    mapper.append(std::vector<std::string>({pimIntf}));

    auto result = bus.call(mapper);
    if(result.is_method_error())
    {
        throw std::runtime_error("ObjectMapper GetObject failed");
    }

    std::map<std::string, std::vector<std::string>> response;
    result.read(response);
    if(response.empty())
    {
        throw std::runtime_error("ObjectMapper GetObject bad response");
    }

    return response.begin()->first;
}

auto callPIM(ObjectMap&& objects)
{
    std::string service;

    try
    {
        service = getPIMService();
        auto bus = sdbusplus::bus::new_default();
        auto pimMsg = bus.new_method_call(
                              service.c_str(),
                              pimPath.c_str(),
                              pimIntf.c_str(),
                              "Notify");
        pimMsg.append(std::move(objects));
        auto result = bus.call(pimMsg);
        if(result.is_method_error())
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

/** @brief API to write parsed VPD to inventory,
 *      for a specifc FRU
 *
 *  @param [in] vpdStore - Store object containing
 *      parsed VPD
 *  @param [in] path - FRU object path
 */
template<Fru F>
void writeFru(const Store& vpdStore, const std::string& path);

% for key in fruDict.iterkeys():
<%
    fru = fruDict[key]
%>\
// Specialization of ${key}
template<>
void writeFru<Fru::${key}>(const Store& vpdStore,
                           const std::string& path)
{
    ObjectMap objects;
    InterfaceMap interfaces;

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    % for interface, properties in fru.iteritems():
<%
        names = interface.split(".")
        intfName = names[0] + names[-1]
%>\
    PropertyMap ${intfName}Props;
        % for name, value in properties.iteritems():
            % if fru and interface and name and value:
<%
                record, keyword = value.split(",")
%>\
    ${intfName}Props["${name}"] =
        vpdStore.get<Record::${record}, record::Keyword::${keyword}>();
            % endif
        % endfor
    interfaces.emplace("${interface}",
                       std::move(${intfName}Props));
    % endfor

    sdbusplus::message::object_path object(path);
    objects.emplace(std::move(object), std::move(interfaces));

    callPIM(std::move(objects));
}

% endfor
} // namespace inventory
} // namespace vpd
} // namespace openpower
