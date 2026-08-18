#pragma once

namespace pgood_chassis_check
{
namespace constants
{
constexpr auto success = 0;
constexpr auto failure = 1;
constexpr auto value_0 = 0;
constexpr auto value_1 = 1;

constexpr auto systemVpdInvPath = "/xyz/openbmc_project/inventory/system";
constexpr auto pimService = "xyz.openbmc_project.Inventory.Manager";

//@todo: use PDI generated header once Position interface gets merged upstream
constexpr auto positionInterface =
    "xyz.openbmc_project.Inventory.Decorator.Position";
constexpr auto positionPropertyName = "Position";

constexpr auto gpioLineBmc0 = "power-good-chassis1";
constexpr auto gpioLineBmc1 = "power-good-chassis2";
constexpr auto consumerName = "pgood-chassis-check";

// D-Bus constants for the PIM Notify call
constexpr auto pimServiceName = "xyz.openbmc_project.Inventory.Manager";
constexpr auto pimPath = "/xyz/openbmc_project/inventory";
constexpr auto pimInterface = "xyz.openbmc_project.Inventory.Manager";
} // namespace constants

} // namespace pgood_chassis_check
