#pragma once

namespace pgood_chassis_check
{
constexpr auto bmcPositionFile = "/run/openbmc/bmc_position";
constexpr auto gpioLineBmc0 = "power-good-chassis1";
constexpr auto gpioLineBmc1 = "power-good-chassis2";
constexpr auto consumerName = "pgood-chassis-check";

// D-Bus target for the set-property call
constexpr auto pimServiceName = "xyz.openbmc_project.Inventory.Manager";
constexpr auto systemInvPath = "/xyz/openbmc_project/inventory/system";

}; // namespace pgood_chassis_check
