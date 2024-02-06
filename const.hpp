#pragma once

#include <cstdint>
#include <filesystem>
#include <iostream>

namespace openpower
{
namespace vpd
{
namespace constants
{

using RecordId = uint8_t;
using RecordOffset = uint16_t;
using RecordSize = uint16_t;
using RecordType = uint16_t;
using RecordLength = uint16_t;
using KwSize = uint8_t;
using PoundKwSize = uint16_t;
using ECCOffset = uint16_t;
using ECCLength = uint16_t;
using LE2ByteData = uint16_t;
using DataOffset = uint16_t;

static constexpr auto NEXT_64_KB = 65536;
static constexpr auto MAC_ADDRESS_LEN_BYTES = 6;
static constexpr auto LAST_KW = "PF";
static constexpr auto POUND_KW = '#';
static constexpr auto POUND_KW_PREFIX = "PD_";
static constexpr auto NUMERIC_KW_PREFIX = "N_";
static constexpr auto UUID_LEN_BYTES = 16;
static constexpr auto UUID_TIME_LOW_END = 8;
static constexpr auto UUID_TIME_MID_END = 13;
static constexpr auto UUID_TIME_HIGH_END = 18;
static constexpr auto UUID_CLK_SEQ_END = 23;

static constexpr auto MB_RESULT_LEN = 19;
static constexpr auto MB_LEN_BYTES = 8;
static constexpr auto MB_YEAR_END = 4;
static constexpr auto MB_MONTH_END = 7;
static constexpr auto MB_DAY_END = 10;
static constexpr auto MB_HOUR_END = 13;
static constexpr auto MB_MIN_END = 16;
static constexpr auto SYSTEM_OBJECT = "/system/chassis/motherboard";
static constexpr auto IBM_LOCATION_CODE_INF = "com.ibm.ipzvpd.Location";
static constexpr auto XYZ_LOCATION_CODE_INF =
    "xyz.openbmc_project.Inventory.Decorator.LocationCode";
constexpr int IPZ_DATA_START = 11;
constexpr uint8_t KW_VAL_PAIR_START_TAG = 0x84;
constexpr uint8_t RECORD_END_TAG = 0x78;
constexpr int UNEXP_LOCATION_CODE_MIN_LENGTH = 4;
constexpr uint8_t EXP_LOCATIN_CODE_MIN_LENGTH = 17;
static constexpr auto SE_KWD_LENGTH = 7;
static constexpr auto INVALID_NODE_NUMBER = -1;
static constexpr auto RAINIER_2U = "50001001.json";
static constexpr auto RAINIER_2U_V2 = "50001001_v2.json";
static constexpr auto RAINIER_4U = "50001000.json";
static constexpr auto RAINIER_4U_V2 = "50001000_v2.json";
static constexpr auto RAINIER_1S4U = "50001002.json";
static constexpr auto EVEREST = "50003000.json";
static constexpr auto EVEREST_V2 = "50003000_v2.json";
static constexpr auto BONNELL = "50004000.json";

constexpr uint8_t KW_VPD_START_TAG = 0x82;
constexpr uint8_t KW_VPD_END_TAG = 0x78;
constexpr uint8_t ALT_KW_VAL_PAIR_START_TAG = 0x90;
constexpr uint8_t KW_VAL_PAIR_END_TAG = 0x79;
constexpr auto MEMORY_VPD_START_TAG = "11S";
constexpr int TWO_BYTES = 2;
constexpr int KW_VPD_DATA_START = 0;
constexpr auto MEMORY_VPD_DATA_START = 416;
constexpr auto FORMAT_11S_LEN = 3;
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

static constexpr auto VALUE_0 = 0;
static constexpr auto VALUE_1 = 1;
static constexpr auto VALUE_2 = 2;
static constexpr auto VALUE_3 = 3;
static constexpr auto VALUE_4 = 4;
static constexpr auto VALUE_5 = 5;
static constexpr auto VALUE_6 = 6;
static constexpr auto VALUE_7 = 7;
static constexpr auto VALUE_8 = 8;

static constexpr auto MASK_BYTE_BITS_01 = 0x03;
static constexpr auto MASK_BYTE_BITS_345 = 0x38;
static constexpr auto MASK_BYTE_BITS_012 = 0x07;
static constexpr auto MASK_BYTE_BITS_567 = 0xE0;
static constexpr auto MASK_BYTE_BITS_0123 = 0x0F;
static constexpr auto MASK_BYTE_BITS_01234 = 0x1F;

static constexpr auto SHIFT_BITS_0 = 0;
static constexpr auto SHIFT_BITS_3 = 3;
static constexpr auto SHIFT_BITS_5 = 5;

static constexpr auto SPD_MODULE_TYPE_DDIMM = 0x0A;
static constexpr auto SPD_DRAM_TYPE_DDR5 = 0x12;
static constexpr auto SPD_DRAM_TYPE_DDR4 = 0x0C;

constexpr auto pimPath = "/xyz/openbmc_project/inventory";
constexpr auto pimIntf = "xyz.openbmc_project.Inventory.Manager";
constexpr auto pimService = "xyz.openbmc_project.Inventory.Manager";
constexpr auto kwdVpdInf = "com.ibm.ipzvpd.VINI";
constexpr auto memVpdInf = "com.ibm.ipzvpd.VINI";
constexpr auto ipzVpdInf = "com.ibm.ipzvpd.";
constexpr auto offsetJsonDirectory = "/var/lib/vpd/";
constexpr auto mapperObjectPath = "/xyz/openbmc_project/object_mapper";
constexpr auto mapperInterface = "xyz.openbmc_project.ObjectMapper";
constexpr auto mapperDestination = "xyz.openbmc_project.ObjectMapper";
constexpr auto loggerService = "xyz.openbmc_project.Logging";
constexpr auto loggerObjectPath = "/xyz/openbmc_project/logging";
constexpr auto loggerCreateInterface = "xyz.openbmc_project.Logging.Create";
constexpr auto errIntfForVPDDefault = "com.ibm.VPD.Error.DefaultValue";
constexpr auto errIntfForInvalidVPD = "com.ibm.VPD.Error.InvalidVPD";
constexpr auto errIntfForVPDMismatch = "com.ibm.VPD.Error.Mismatch";
constexpr auto errIntfForStreamFail = "com.ibm.VPD.Error.InvalidEepromPath";
constexpr auto errIntfForEccCheckFail = "com.ibm.VPD.Error.EccCheckFailed";
constexpr auto errIntfForJsonFailure = "com.ibm.VPD.Error.InvalidJson";
constexpr auto errIntfForBusFailure = "com.ibm.VPD.Error.DbusFailure";
constexpr auto errIntfForInvalidSystemType =
    "com.ibm.VPD.Error.UnknownSytemType";
constexpr auto errIntfForEssentialFru = "com.ibm.VPD.Error.RequiredFRUMissing";
constexpr auto errIntfForGpioError = "com.ibm.VPD.Error.GPIOError";
constexpr auto motherBoardInterface =
    "xyz.openbmc_project.Inventory.Item.Board.Motherboard";
constexpr auto systemVpdFilePath = "/sys/bus/i2c/drivers/at24/8-0050/eeprom";
constexpr auto i2cPathPrefix = "/sys/bus/i2c/drivers/";
constexpr auto spiPathPrefix = "/sys/bus/spi/drivers/";
constexpr auto at24driver = "at24";
constexpr auto at25driver = "at25";
constexpr auto ee1004driver = "ee1004";
constexpr auto invItemIntf = "xyz.openbmc_project.Inventory.Item";
constexpr auto invAssetIntf = "xyz.openbmc_project.Inventory.Decorator.Asset";
constexpr auto invOperationalStatusIntf =
    "xyz.openbmc_project.State.Decorator.OperationalStatus";
static constexpr std::uintmax_t MAX_VPD_SIZE = 65504;

namespace lengths
{
enum Lengths
{
    RECORD_NAME = 4,
    KW_NAME = 2,
    RECORD_OFFSET = 2,
    RECORD_MIN = 44,
    RECORD_LENGTH = 2,
    RECORD_ECC_OFFSET = 2,
    VHDR_ECC_LENGTH = 11,
    VHDR_RECORD_LENGTH = 44
}; // enum Lengths
} // namespace lengths

namespace offsets
{
enum Offsets
{
    VHDR = 17,
    VHDR_TOC_ENTRY = 29,
    VTOC_PTR = 35,
    VTOC_REC_LEN = 37,
    VTOC_ECC_OFF = 39,
    VTOC_ECC_LEN = 41,
    VTOC_DATA = 13,
    VHDR_ECC = 0,
    VHDR_RECORD = 11
};
} // namespace offsets

namespace eccStatus
{
enum Status
{
    SUCCESS = 0,
    FAILED = -1
};
} // namespace eccStatus

/**
 * @brief Types of VPD
 */
enum vpdType
{
    IPZ_VPD,                /**< IPZ VPD type */
    KEYWORD_VPD,            /**< Keyword VPD type */
    DDR4_DDIMM_MEMORY_VPD,  /**< DDR4 DDIMM Memory VPD type */
    DDR5_DDIMM_MEMORY_VPD,  /**< DDR5 DDIMM Memory VPD type */
    DDR4_ISDIMM_MEMORY_VPD, /**< DDR4 ISDIMM Memory VPD type */
    DDR5_ISDIMM_MEMORY_VPD, /**< DDR5 ISDIMM Memory VPD type */
    INVALID_VPD_FORMAT      /**< Invalid VPD type Format */
};

enum PelSeverity
{
    NOTICE,
    INFORMATIONAL,
    DEBUG,
    WARNING,
    CRITICAL,
    EMERGENCY,
    ALERT,
    ERROR
};

} // namespace constants
} // namespace vpd
} // namespace openpower
