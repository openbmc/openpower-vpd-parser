#pragma once

namespace pgood_chassis_check
{
namespace constants
{
constexpr auto success = 0;
constexpr auto failure = 1;
constexpr auto systemVpdInvPath = "/xyz/openbmc_project/inventory/system";
constexpr auto pimService = "xyz.openbmc_project.Inventory.Manager";
} // namespace constants

} // namespace pgood_chassis_check
