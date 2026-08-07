#pragma once

namespace pgood_chassis_check
{
constexpr auto bmcPositionFile = "/run/openbmc/bmc_position";
constexpr auto vpdRoleFile = "/run/wait-vpd-role";
constexpr auto gpioLineBmc0 = "power-good-chassis1";
constexpr auto gpioLineBmc1 = "power-good-chassis2";
constexpr auto consumerName = "pgood-chassis-check";
constexpr auto roleActive = "active";
constexpr auto rolePoweron = "poweron";
}; // namespace pgood_chassis_check
