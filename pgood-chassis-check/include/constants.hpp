#pragma once

namespace pgood_chassis_check
{
namespace constants
{
constexpr auto success = 0;
constexpr auto failure = 1;
<<<<<<< HEAD
constexpr auto systemVpdInvPath = "/xyz/openbmc_project/inventory/system";
constexpr auto pimService = "xyz.openbmc_project.Inventory.Manager";
=======
constexpr auto bmcPositionFile = "/run/openbmc/bmc_position";
constexpr auto gpioLineBmc0 = "power-good-chassis1";
constexpr auto gpioLineBmc1 = "power-good-chassis2";
constexpr auto consumerName = "pgood-chassis-check";

// D-Bus constants for the PIM Notify call
constexpr auto pimServiceName = "xyz.openbmc_project.Inventory.Manager";
constexpr auto pimPath = "/xyz/openbmc_project/inventory";
constexpr auto pimInterface = "xyz.openbmc_project.Inventory.Manager";
>>>>>>> b5e421a (pgood-chassis-check:publish PowerState property)
} // namespace constants

} // namespace pgood_chassis_check
