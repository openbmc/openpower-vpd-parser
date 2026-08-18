#pragma once

namespace pgood_chassis_check
{
namespace constants
{
constexpr auto success = 0;
constexpr auto failure = 1;
constexpr auto systemVpdInvPath = "/xyz/openbmc_project/inventory/system";
constexpr auto pimService = "xyz.openbmc_project.Inventory.Manager";

//@todo: use PDI generated header once Position interface gets merged upstream
constexpr auto positionInterface =
    "xyz.openbmc_project.Inventory.Decorator.Position";
constexpr auto positionPropertyName = "Position";

} // namespace constants

} // namespace pgood_chassis_check
