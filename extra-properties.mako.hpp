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
using namespace openpower::vpd::types;

const std::map<Path, InterfaceMap> objects = {
% for path in dict.keys():
<%
    interfaces = dict[path]
%>\
    {"${path}",{
    % for interface,properties in interfaces.items():
        {"${interface}",{
        % for property,value in properties.items():
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
