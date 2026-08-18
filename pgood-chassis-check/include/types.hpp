#pragma once
#include <xyz/openbmc_project/State/Decorator/PowerState/common.hpp>

#include <map>
#include <string>
#include <variant>

namespace pgood_chassis_check
{
namespace types
{
using PowerStateIface =
    sdbusplus::common::xyz::openbmc_project::state::decorator::PowerState;
using PropertyMap = std::map<std::string, std::variant<std::string>>;
using InterfaceMap = std::map<std::string, PropertyMap>;
using ObjectMap = std::map<sdbusplus::object_path, InterfaceMap>;

enum class GpioValue : int8_t
{
    INVALID_VALUE = -1,
    OFF,
    ON
};

enum class BmcPosition : uint8_t
{
    POSITION_0,
    POSITION_1,
    DEFAULT = POSITION_0
};
} // namespace types
} // namespace pgood_chassis_check
