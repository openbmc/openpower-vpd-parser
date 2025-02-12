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
constexpr auto viniInf = "com.ibm.ipzvpd.VINI";
constexpr auto locationCodeInf = "com.ibm.ipzvpd.Location";
constexpr auto assetInf = "xyz.openbmc_project.Inventory.Decorator.Asset";
constexpr auto objectMapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto objectMapperObjectPath = "/xyz/openbmc_project/object_mapper";
constexpr auto objectMapperInfName = "xyz.openbmc_project.ObjectMapper";
constexpr auto chassisStateManagerService = "xyz.openbmc_project.State.Chassis";
constexpr auto chassisStateManagerObjectPath =
    "/xyz/openbmc_project/state/chassis0";
constexpr auto chassisStateManagerInfName = "xyz.openbmc_project.State.Chassis";
constexpr auto networkInf =
    "xyz.openbmc_project.Inventory.Item.NetworkInterface";
constexpr auto pcieSlotInf = "xyz.openbmc_project.Inventory.Item.PCIeSlot";
constexpr auto slotNumInf = "xyz.openbmc_project.Inventory.Decorator.Slot";
constexpr auto i2cDeviceInf =
    "xyz.openbmc_project.Inventory.Decorator.I2CDevice";
} // namespace constants
} // namespace vpd
