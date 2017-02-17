## This file is a template.  The comment below is emitted
## into the rendered file; feel free to edit this file.
// WARNING: Generated header. Do not edit!

#pragma once

#include <string>
#include <map>
#include "types.hpp"

namespace openpower
{
namespace vpd
{
namespace inventory
{
namespace extra
{

const std::map<Path, InterfaceMap> objects = {
% for path in dict.iterkeys():
<%
    interfaces = dict[path]
%>\
    {"${path}",{
    % for interface,properties in interfaces.iteritems():
        {"${interface}",{
        % for property,value in properties.iteritems():
            {"${property}", ${value}},
        % endfor
        }},
    % endfor
    }},
% endfor
};

} // namespace extra
} // namespace inventory
} // namespace vpd
} // namespace openpower
