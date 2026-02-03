#pragma once

#include <cstdint>
#include <iostream>
#include <vector>
namespace vpd
{
namespace constants
{
static constexpr auto KEYWORD_SIZE = 2;
static constexpr auto RECORD_SIZE = 4;

static constexpr uint8_t IPZ_DATA_START = 11;
static constexpr uint8_t IPZ_DATA_START_TAG = 0x84;
static constexpr uint8_t IPZ_RECORD_END_TAG = 0x78;

static constexpr uint8_t KW_VPD_DATA_START = 0;
static constexpr uint8_t KW_VPD_START_TAG = 0x82;
static constexpr uint8_t KW_VPD_PAIR_START_TAG = 0x84;
static constexpr uint8_t ALT_KW_VPD_PAIR_START_TAG = 0x90;
static constexpr uint8_t KW_VPD_END_TAG = 0x78;
static constexpr uint8_t KW_VAL_PAIR_END_TAG = 0x79;
static constexpr uint8_t AMM_ENABLED_IN_VPD = 2;
static constexpr uint8_t AMM_DISABLED_IN_VPD = 1;

static constexpr auto DDIMM_11S_BARCODE_START = 416;
static constexpr auto DDIMM_11S_BARCODE_START_TAG = "11S";
static constexpr auto DDIMM_11S_FORMAT_LEN = 3;
static constexpr auto DDIMM_11S_BARCODE_LEN = 26;
static constexpr auto PART_NUM_LEN = 7;
static constexpr auto SERIAL_NUM_LEN = 12;
static constexpr auto CCIN_LEN = 4;
static constexpr auto CONVERT_MB_TO_KB = 1024;
static constexpr auto CONVERT_GB_TO_KB = 1024 * 1024;

static constexpr auto SPD_BYTE_2 = 2;
static constexpr auto SPD_BYTE_3 = 3;
static constexpr auto SPD_BYTE_4 = 4;
static constexpr auto SPD_BYTE_6 = 6;
static constexpr auto SPD_BYTE_12 = 12;
static constexpr auto SPD_BYTE_13 = 13;
static constexpr auto SPD_BYTE_18 = 18;
static constexpr auto SPD_BYTE_234 = 234;
static constexpr auto SPD_BYTE_235 = 235;
static constexpr auto SPD_BYTE_BIT_0_3_MASK = 0x0F;
static constexpr auto SPD_BYTE_MASK = 0xFF;
static constexpr auto SPD_MODULE_TYPE_DDIMM = 0x0A;
static constexpr auto SPD_DRAM_TYPE_DDR5 = 0x12;
static constexpr auto SPD_DRAM_TYPE_DDR4 = 0x0C;

static constexpr auto JEDEC_SDRAM_CAP_MASK = 0x0F;
static constexpr auto JEDEC_PRI_BUS_WIDTH_MASK = 0x07;
static constexpr auto JEDEC_SDRAM_WIDTH_MASK = 0x07;
static constexpr auto JEDEC_NUM_RANKS_MASK = 0x38;
static constexpr auto JEDEC_DIE_COUNT_MASK = 0x70;
static constexpr auto JEDEC_SINGLE_LOAD_STACK = 0x02;
static constexpr auto JEDEC_SIGNAL_LOADING_MASK = 0x03;

static constexpr auto JEDEC_SDRAMCAP_MULTIPLIER = 256;
static constexpr auto JEDEC_PRI_BUS_WIDTH_MULTIPLIER = 8;
static constexpr auto JEDEC_SDRAM_WIDTH_MULTIPLIER = 4;
static constexpr auto JEDEC_SDRAMCAP_RESERVED = 7;
static constexpr auto JEDEC_RESERVED_BITS = 3;
static constexpr auto JEDEC_DIE_COUNT_RIGHT_SHIFT = 4;

static constexpr auto LAST_KW = "PF";
static constexpr auto POUND_KW = '#';
static constexpr auto POUND_KW_PREFIX = "PD_";
static constexpr auto MB_YEAR_END = 4;
static constexpr auto MB_MONTH_END = 7;
static constexpr auto MB_DAY_END = 10;
static constexpr auto MB_HOUR_END = 13;
static constexpr auto MB_MIN_END = 16;
static constexpr auto MB_RESULT_LEN = 19;
static constexpr auto MB_LEN_BYTES = 8;
static constexpr auto UUID_LEN_BYTES = 16;
static constexpr auto UUID_TIME_LOW_END = 8;
static constexpr auto UUID_TIME_MID_END = 13;
static constexpr auto UUID_TIME_HIGH_END = 18;
static constexpr auto UUID_CLK_SEQ_END = 23;
static constexpr auto MAC_ADDRESS_LEN_BYTES = 6;
static constexpr auto ONE_BYTE = 1;
static constexpr auto TWO_BYTES = 2;

static constexpr auto VALUE_0 = 0;
static constexpr auto VALUE_1 = 1;
static constexpr auto VALUE_2 = 2;
static constexpr auto VALUE_3 = 3;
static constexpr auto VALUE_4 = 4;
static constexpr auto VALUE_5 = 5;
static constexpr auto VALUE_6 = 6;
static constexpr auto VALUE_7 = 7;
static constexpr auto VALUE_8 = 8;
static constexpr auto VALUE_21 = 21;

static constexpr auto MASK_BYTE_BITS_01 = 0x03;
static constexpr auto MASK_BYTE_BITS_345 = 0x38;
static constexpr auto MASK_BYTE_BITS_012 = 0x07;
static constexpr auto MASK_BYTE_BITS_567 = 0xE0;
static constexpr auto MASK_BYTE_BITS_01234 = 0x1F;

static constexpr auto MASK_BYTE_BIT_6 = 0x40;
static constexpr auto MASK_BYTE_BIT_7 = 0x80;

static constexpr auto SHIFT_BITS_0 = 0;
static constexpr auto SHIFT_BITS_3 = 3;
static constexpr auto SHIFT_BITS_5 = 5;

static constexpr auto ASCII_OF_SPACE = 32;

// Size of 8 EQs' in CP00's PG keyword
static constexpr auto SIZE_OF_8EQ_IN_PG = 24;

// Zero based index position of first EQ in CP00's PG keyword
static constexpr auto INDEX_OF_EQ0_IN_PG = 97;

static constexpr auto HEX_VALUE_50 = 0x50;
static constexpr auto HEX_VALUE_30 = 0x30;
static constexpr auto HEX_VALUE_10 = 0x10;
static constexpr auto HEX_VALUE_00 = 0x00;

constexpr auto systemInvPath = "/xyz/openbmc_project/inventory/system";
constexpr auto pimPath = "/xyz/openbmc_project/inventory";
constexpr auto pimIntf = "xyz.openbmc_project.Inventory.Manager";
constexpr auto ipzVpdInf = "com.ibm.ipzvpd.";
constexpr auto kwdVpdInf = "com.ibm.ipzvpd.VINI";
constexpr auto vsysInf = "com.ibm.ipzvpd.VSYS";
constexpr auto utilInf = "com.ibm.ipzvpd.UTIL";
constexpr auto vcenInf = "com.ibm.ipzvpd.VCEN";
constexpr auto viniInf = "com.ibm.ipzvpd.VINI";
constexpr auto vsbpInf = "com.ibm.ipzvpd.VSBP";
constexpr auto kwdCCIN = "CC";
constexpr auto kwdRG = "RG";
constexpr auto kwdAMM = "MM";
constexpr auto kwdClearNVRAM_CreateLPAR = "BA";
constexpr auto kwdKeepAndClear = "BA";
constexpr auto kwdFC = "FC";
constexpr auto kwdTM = "TM";
constexpr auto kwdSE = "SE";
constexpr auto kwdHW = "HW";
constexpr auto kwdIM = "IM";
constexpr auto kwdPN = "PN";
constexpr auto kwdFN = "FN";
constexpr auto recVSYS = "VSYS";
constexpr auto recVCEN = "VCEN";
constexpr auto recVSBP = "VSBP";
constexpr auto locationCodeInf = "com.ibm.ipzvpd.Location";
constexpr auto xyzLocationCodeInf =
    "xyz.openbmc_project.Inventory.Decorator.LocationCode";
constexpr auto operationalStatusInf =
    "xyz.openbmc_project.State.Decorator.OperationalStatus";
constexpr auto enableInf = "xyz.openbmc_project.Object.Enable";
constexpr auto assetInf = "xyz.openbmc_project.Inventory.Decorator.Asset";
constexpr auto inventoryItemInf = "xyz.openbmc_project.Inventory.Item";
constexpr auto pldmServiceName = "xyz.openbmc_project.PLDM";
constexpr auto pimServiceName = "xyz.openbmc_project.Inventory.Manager";
constexpr auto biosConfigMgrObjPath =
    "/xyz/openbmc_project/bios_config/manager";
constexpr auto biosConfigMgrService = "xyz.openbmc_project.BIOSConfigManager";
constexpr auto biosConfigMgrInterface =
    "xyz.openbmc_project.BIOSConfig.Manager";
constexpr auto objectMapperService = "xyz.openbmc_project.ObjectMapper";
constexpr auto objectMapperPath = "/xyz/openbmc_project/object_mapper";
constexpr auto objectMapperInf = "xyz.openbmc_project.ObjectMapper";
constexpr auto systemVpdInvPath = systemInvPath;
constexpr auto motherboardInterface =
    "xyz.openbmc_project.Inventory.Item.Board.Motherboard";
constexpr auto assetTagInf = "xyz.openbmc_project.Inventory.Decorator.AssetTag";
constexpr auto hostObjectPath = "/xyz/openbmc_project/state/host0";
constexpr auto hostInterface = "xyz.openbmc_project.State.Host";
constexpr auto hostService = "xyz.openbmc_project.State.Host";
constexpr auto hostRunningState =
    "xyz.openbmc_project.State.Host.HostState.Running";
constexpr auto imageUpdateService = "xyz.openbmc_project.Software.BMC.Updater";
constexpr auto imagePrirotyInf =
    "xyz.openbmc_project.Software.RedundancyPriority";
constexpr auto imageExtendedVerInf =
    "xyz.openbmc_project.Software.ExtendedVersion";
constexpr auto functionalImageObjPath =
    "/xyz/openbmc_project/software/functional";
constexpr auto associationInterface = "xyz.openbmc_project.Association";
constexpr auto powerVsImagePrefix_MY = "MY";
constexpr auto powerVsImagePrefix_MZ = "MZ";
constexpr auto powerVsImagePrefix_NY = "NY";
constexpr auto powerVsImagePrefix_NZ = "NZ";
constexpr auto badVpdDir = "/var/lib/vpd/dumps/";
constexpr auto bmcPositionFile = "/run/openbmc/bmc_position";

static constexpr auto BD_YEAR_END = 4;
static constexpr auto BD_MONTH_END = 7;
static constexpr auto BD_DAY_END = 10;
static constexpr auto BD_HOUR_END = 13;

constexpr uint8_t UNEXP_LOCATION_CODE_MIN_LENGTH = 4;
constexpr uint8_t EXP_LOCATION_CODE_MIN_LENGTH = 17;
static constexpr auto SE_KWD_LENGTH = 7;
static constexpr auto INVALID_NODE_NUMBER = -1;

static constexpr auto CMD_BUFFER_LENGTH = 256;

// To be explicitly used for string comparision.
static constexpr auto STR_CMP_SUCCESS = 0;

// Just a random value. Can be adjusted as required.
static constexpr uint8_t MAX_THREADS = 10;

static constexpr auto FAILURE = -1;
static constexpr auto SUCCESS = 0;

constexpr auto bmcStateService = "xyz.openbmc_project.State.BMC";
constexpr auto bmcZeroStateObject = "/xyz/openbmc_project/state/bmc0";
constexpr auto bmcStateInterface = "xyz.openbmc_project.State.BMC";
constexpr auto currentBMCStateProperty = "CurrentBMCState";
constexpr auto bmcReadyState = "xyz.openbmc_project.State.BMC.BMCState.Ready";

static constexpr auto eventLoggingServiceName = "xyz.openbmc_project.Logging";
static constexpr auto eventLoggingObjectPath = "/xyz/openbmc_project/logging";
static constexpr auto eventLoggingInterface =
    "xyz.openbmc_project.Logging.Create";

static constexpr auto systemdService = "org.freedesktop.systemd1";
static constexpr auto systemdObjectPath = "/org/freedesktop/systemd1";
static constexpr auto systemdManagerInterface =
    "org.freedesktop.systemd1.Manager";

static constexpr auto vpdCollectionInterface =
    "xyz.openbmc_project.Common.Progress";

// enumerated values of collection Status D-bus property defined under
// xyz.openbmc_project.Common.Progress interface.
static constexpr auto vpdCollectionCompleted =
    "xyz.openbmc_project.Common.Progress.OperationStatus.Completed";
static constexpr auto vpdCollectionFailed =
    "xyz.openbmc_project.Common.Progress.OperationStatus.Failed";
static constexpr auto vpdCollectionInProgress =
    "xyz.openbmc_project.Common.Progress.OperationStatus.InProgress";
static constexpr auto vpdCollectionNotStarted =
    "xyz.openbmc_project.Common.Progress.OperationStatus.NotStarted";
static constexpr auto power_vs_50003_json =
    "/usr/share/vpd/50003_power_vs.json";
static constexpr auto power_vs_50001_json =
    "/usr/share/vpd/50001_power_vs.json";

static constexpr auto rbmcPositionInterface =
    "xyz.openbmc_project.Inventory.Decorator.Position";
static std::vector<uint8_t> rbmcPrototypeSystemImValue{118, 0, 32, 0};

static constexpr auto fileModeDirectoryPath = "/var/lib/vpd/file";
static constexpr auto pimBackupPath =
    "/var/lib/phosphor-data-sync/bmc_data_bkp/var/lib/phosphor-inventory-manager";
static constexpr auto pimPrimaryPath = "/var/lib/phosphor-inventory-manager";
} // namespace constants
} // namespace vpd
