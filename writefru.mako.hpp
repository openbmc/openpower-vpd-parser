## This file is a template.  The comment below is emitted
## into the rendered file; feel free to edit this file.
// WARNING: Generated header. Do not edit!


#pragma once

#include <map>
#include <iostream>
#include "defines.hpp"
#include "store.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "extra-properties-gen.hpp"

namespace openpower
{
namespace vpd
{
namespace inventory
{

/** @brief API to write parsed VPD to inventory,
 *      for a specific FRU
 *
 *  @param [in] vpdStore - Store object containing
 *      parsed VPD
 *  @param [in] path - FRU object path
 */
template<Fru F>
void writeFru(const Store& vpdStore, const std::string& path) {
    throw std::runtime_error("Not implemented");
}

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
    auto iter = extra::objects.find(path);

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    % for interface, properties in fru.iteritems():
<%
        names = interface.split(".")
        intfName = names[0] + names[-1]
%>\
    PropertyMap ${intfName}Props;
        % if properties:
            % for name, value in properties.iteritems():
                % if fru and interface and name and value:
<%
                record, keyword = name.split(",")
%>\
    if (vpdStore.exists<Record::${record}, record::Keyword::${keyword}>())
    {
        ${intfName}Props["${value}"] =
            vpdStore.get<Record::${record}, record::Keyword::${keyword}>();
    }
                % endif
            % endfor
        % endif
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        auto propIter = (iter->second).find("${interface}");
        if((iter->second).end() != propIter)
        {
            for(const auto& map : propIter->second)
            {
                ${intfName}Props[map.first] = map.second;
            }
        }
    }
    interfaces.emplace("${interface}",
                       std::move(${intfName}Props));
    % endfor

    sdbusplus::message::object_path object(path);
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        for(const auto& entry : iter->second)
        {
            if(interfaces.end() == interfaces.find(entry.first))
            {
                interfaces.emplace(entry.first, entry.second);
            }
        }
    }
    objects.emplace(std::move(object), std::move(interfaces));

    callPIM(std::move(objects));
}

% endfor
} // namespace inventory
} // namespace vpd
} // namespace openpower
