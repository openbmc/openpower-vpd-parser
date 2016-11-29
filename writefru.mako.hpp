## This file is a template.  The comment below is emitted
## into the rendered file; feel free to edit this file.
// WARNING: Generated header. Do not edit!


#pragma once

#include <map>
#include <iostream>
#include "defines.hpp"
#include "store.hpp"

namespace openpower
{
namespace vpd
{
namespace inventory
{

using Inner = Parsed::mapped_type;
using Outer = std::map<std::string, Inner>;

// TODO: Remove once the call to inventory manager is added
auto print = [](Outer&& object, const std::string& path)
{
    std::cout << "\n";
    std::cout << path << "\n";
    std::cout << "\n";
    for(const auto& o : object)
    {
        std::cout << o.first << "\n";
        for(const auto& i : o.second)
        {
            std::cout << i.first << " : " << i.second << "\n";
        }
        std::cout << "\n";
    }
};

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
    Outer object;

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    % for interfaces, properties in fru.iteritems():
<%
        interface = interfaces.split(".")
        intfName = interface[0] + interface[-1]
%>\
    Inner ${intfName};
        % for name, value in properties.iteritems():
            % if fru and interfaces and name and value:
<%
                record, keyword = value.split(",")
%>\
    ${intfName}["${name}"] =
        vpdStore.get<Record::${record}, record::Keyword::${keyword}>();
            % endif
        % endfor
    object.emplace("${interfaces}",
                   std::move(${intfName}));
    % endfor

    // TODO: Need integration with inventory manager, print serialized dbus
    // object for now.
    print(std::move(object), path);
}

% endfor
} // namespace inventory
} // namespace vpd
} // namespace openpower
