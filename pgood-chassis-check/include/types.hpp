#pragma once
#include <xyz/openbmc_project/State/Decorator/PowerState/common.hpp>

namespace pgood_chassis_check
{
namespace types
{
using PowerStateIface =
    sdbusplus::common::xyz::openbmc_project::state::decorator::PowerState;

using PositionIface =
    sdbusplus::common::xyz::openbmc_project::inventory::decorator::Position;
using DbusVariantType =
    std::variant<std::vector<uint16_t>, bool, std::vector<uint32_t>,
                 std::string, BinaryVector, size_t>;

enum class GpioValue : int8_t
{
    INVALID_VALUE = -1,
    OFF,
    ON
};

enum class BmcPosition : int8_t
{
    INVALID_VALUE = -1,
    POSITION_0,
    POSITION_1
};
} // namespace types
} // namespace pgood_chassis_check
