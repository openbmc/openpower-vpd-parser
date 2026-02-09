#pragma once

#include "types.hpp"

namespace vpd
{
namespace config
{

/**
 * @brief Map of IM to HW version.
 *
 * The map holds HW version corresponding to a given IM value.
 * To add a new system, just update the below map.
 * {IM value, {Default, {HW_version, version}}}
 */
types::SystemTypeMap systemType{
    {"50001001", {"50001001_v2", {{"0001", ""}}}},
    {"50001000", {"50001000_v2", {{"0001", ""}}}},
    {"50001002", {"50001002", {}}},
    {"50003000",
     {"50003000_v2", {{"000A", ""}, {"000B", ""}, {"000C", ""}, {"0014", ""}}}},
    {"50004000", {"50004000", {}}},
    {"60001001", {"60001001_v2", {{"0001", ""}}}},
    {"60001000", {"60001000_v2", {{"0001", ""}}}},
    {"60001002", {"60001002", {}}},
    {"60002000",
     {"60002000_v2", {{"000A", ""}, {"000B", ""}, {"000C", ""}, {"0014", ""}}}},
    {"60004000", {"60004000", {}}},
    {"76002000", {"76002000", {}}},
    {"76002001", {"76002001", {}}},
    {"70001000", {"70001000", {}}}};
} // namespace config
} // namespace vpd
