#pragma once
#include <xyz/openbmc_project/State/Decorator/PowerState/common.hpp>

namespace pgood_chassis_check
{
using PowerStateIface =
    sdbusplus::common::xyz::openbmc_project::state::decorator::PowerState;
using PropertyMap = std::map<std::string, std::variant<std::string>>;
using InterfaceMap = std::map<std::string, PropertyMap>;
using ObjectMap = std::map<sdbusplus::object_path, InterfaceMap>;
}; // namespace pgood_chassis_check
