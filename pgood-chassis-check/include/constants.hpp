#pragma once

namespace pgood_chassis_check
{
namespace constants
{
constexpr auto success = 0;
constexpr auto failure = 1;
constexpr auto bmcPositionFile = "/run/openbmc/bmc_position";
constexpr auto gpioLineBmc0 = "power-good-chassis1";
constexpr auto gpioLineBmc1 = "power-good-chassis2";
constexpr auto consumerName = "pgood-chassis-check";
} // namespace constants

} // namespace pgood_chassis_check
