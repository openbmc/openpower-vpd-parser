#pragma once

#include <cstdint>

namespace vpd
{
namespace constants
{
static constexpr auto KEYWORD_SIZE = 2;
static constexpr auto RECORD_SIZE = 4;
static constexpr auto INDENTATION = 4;
static constexpr auto SUCCESS = 0;
static constexpr auto FAILURE = -1;

// To be explicitly used for string comparison.
static constexpr auto STR_CMP_SUCCESS = 0;

constexpr auto inventoryManagerService =
    "xyz.openbmc_project.Inventory.Manager";
constexpr auto baseInventoryPath = "/xyz/openbmc_project/inventory";
constexpr auto ipzVpdInfPrefix = "com.ibm.ipzvpd.";

constexpr auto vpdManagerService = "com.ibm.VPD.Manager";
constexpr auto vpdManagerObjectPath = "/com/ibm/VPD/Manager";
constexpr auto vpdManagerInfName = "com.ibm.VPD.Manager";
constexpr auto inventoryItemInf = "xyz.openbmc_project.Inventory.Item";
constexpr auto kwdVpdInf = "com.ibm.ipzvpd.VINI";
constexpr auto locationCodeInf = "com.ibm.ipzvpd.Location";
constexpr auto assetInf = "xyz.openbmc_project.Inventory.Decorator.Asset";
constexpr auto objectMapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto objectMapperObjectPath = "/xyz/openbmc_project/object_mapper";
constexpr auto objectMapperInfName = "xyz.openbmc_project.ObjectMapper";
constexpr auto biosConfigMgrObjPath =
    "/xyz/openbmc_project/bios_config/manager";
constexpr auto biosConfigMgrService = "xyz.openbmc_project.BIOSConfigManager";
constexpr auto biosConfigMgrInterface =
    "xyz.openbmc_project.BIOSConfig.Manager";

static constexpr auto VALUE_0 = 0;
static constexpr auto VALUE_1 = 1;
static constexpr auto VALUE_2 = 2;
static constexpr auto VALUE_3 = 3;
static constexpr auto VALUE_4 = 4;
static constexpr auto VALUE_5 = 5;
static constexpr auto VALUE_6 = 6;
static constexpr auto VALUE_7 = 7;
static constexpr auto VALUE_8 = 8;
static constexpr auto VALUE_32 = 32;

static constexpr auto enabledString = "enabled";
} // namespace constants
} // namespace vpd
