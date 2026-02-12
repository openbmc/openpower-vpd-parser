#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vpd
{
namespace constants
{
static constexpr auto POUND_KW = '#';
static constexpr auto POUND_KW_PREFIX = "PD_";
static constexpr auto NUMERIC_KW_PREFIX = "N_";

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
constexpr auto pimPersistPath = "/var/lib/phosphor-inventory-manager";
constexpr auto ipzVpdInfPrefix = "com.ibm.ipzvpd.";

constexpr auto vpdManagerService = "com.ibm.VPD.Manager";
constexpr auto vpdManagerObjectPath = "/com/ibm/VPD/Manager";
constexpr auto vpdManagerInfName = "com.ibm.VPD.Manager";
constexpr auto inventoryItemInf = "xyz.openbmc_project.Inventory.Item";
constexpr auto viniInf = "com.ibm.ipzvpd.VINI";
constexpr auto xyzLocationCodeInf =
    "xyz.openbmc_project.Inventory.Decorator.LocationCode";
constexpr auto assetInf = "xyz.openbmc_project.Inventory.Decorator.Asset";
constexpr auto objectMapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto objectMapperObjectPath = "/xyz/openbmc_project/object_mapper";
constexpr auto objectMapperInfName = "xyz.openbmc_project.ObjectMapper";
constexpr auto chassisStateManagerService = "xyz.openbmc_project.State.Chassis";
constexpr auto chassisStateManagerObjectPath =
    "/xyz/openbmc_project/state/chassis0";
constexpr auto chassisStateManagerInfName = "xyz.openbmc_project.State.Chassis";
constexpr auto dbusService = "org.freedesktop.DBus";
constexpr auto dbusObjectPath = "/org/freedesktop/DBus";
constexpr auto dbusInterface = "org.freedesktop.DBus";
constexpr auto biosConfigMgrService = "xyz.openbmc_project.BIOSConfigManager";
constexpr auto networkInf =
    "xyz.openbmc_project.Inventory.Item.NetworkInterface";
constexpr auto pcieSlotInf = "xyz.openbmc_project.Inventory.Item.PCIeSlot";
constexpr auto slotNumInf = "xyz.openbmc_project.Inventory.Decorator.Slot";
constexpr auto i2cDeviceInf =
    "xyz.openbmc_project.Inventory.Decorator.I2CDevice";
constexpr auto vpdManagerProcessName = "vpd-manager";
constexpr auto biosConfigMgrObjPath =
    "/xyz/openbmc_project/bios_config/manager";
constexpr auto biosConfigMgrInterface =
    "xyz.openbmc_project.BIOSConfig.Manager";
constexpr auto waitVpdParserProcessName = "wait-vpd-parsers";

constexpr auto KwdIM = "IM";
constexpr auto badVpdPath = "/var/lib/vpd/dumps";

// Valid IM values list.
static std::vector<std::string> validImValues{
    "0x50001000", "0x50001001", "0x50001002", "0x50003000", "0x50004000",
    "0x60001000", "0x60001001", "0x60001002", "0x60002000", "0x60004000",
    "0x76002000", "0x76002001", "0x70001000"};

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
} // namespace constants
} // namespace vpd
