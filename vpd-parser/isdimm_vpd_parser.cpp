#include "isdimm_vpd_parser.hpp"

#include <iostream>
#include <numeric>
#include <string>

namespace openpower
{
namespace vpd
{
namespace isdimm
{
namespace parser
{
static constexpr auto SPD_JEDEC_DDR4_SDRAM_CAP_MASK = 0x0F;
static constexpr auto SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MASK = 0x07;
static constexpr auto SPD_JEDEC_DDR4_SDRAM_WIDTH_MASK = 0x07;
static constexpr auto SPD_JEDEC_DDR4_NUM_RANKS_MASK = 0x38;
static constexpr auto SPD_JEDEC_DDR4_DIE_COUNT_MASK = 0x70;
static constexpr auto SPD_JEDEC_DDR4_SINGLE_LOAD_STACK = 0x02;
static constexpr auto SPD_JEDEC_DDR4_SIGNAL_LOADING_MASK = 0x03;

static constexpr auto SPD_JEDEC_DDR4_SDRAMCAP_MULTIPLIER = 256;
static constexpr auto SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MULTIPLIER = 8;
static constexpr auto SPD_JEDEC_DDR4_SDRAM_WIDTH_MULTIPLIER = 4;
static constexpr auto SPD_JEDEC_DDR4_SDRAMCAP_RESERVED = 8;
static constexpr auto SPD_JEDEC_DDR4_4_RESERVED_BITS = 4;
static constexpr auto SPD_JEDEC_DDR4_3_RESERVED_BITS = 3;
static constexpr auto SPD_JEDEC_DDR4_DIE_COUNT_RIGHT_SHIFT = 4;

static constexpr auto SPD_JEDEC_DDR4_MFG_ID_MSB_OFFSET = 321;
static constexpr auto SPD_JEDEC_DDR4_MFG_ID_LSB_OFFSET = 320;
static constexpr auto SPD_JEDEC_DDR4_SN_BYTE0_OFFSET = 325;
static constexpr auto SPD_JEDEC_DDR4_SN_BYTE1_OFFSET = 326;
static constexpr auto SPD_JEDEC_DDR4_SN_BYTE2_OFFSET = 327;
static constexpr auto SPD_JEDEC_DDR4_SN_BYTE3_OFFSET = 328;
static constexpr auto SPD_JEDEC_DDR4_SDRAM_DENSITY_BANK_OFFSET = 4;
static constexpr auto SPD_JEDEC_DDR4_SDRAM_ADDR_OFFSET = 5;
static constexpr auto SPD_JEDEC_DDR4_DRAM_PRI_PACKAGE_OFFSET = 6;
static constexpr auto SPD_JEDEC_DDR4_DRAM_MODULE_ORG_OFFSET = 12;

// DDR5 JEDEC specification constants
static constexpr auto SPD_JEDEC_DDR5_SUB_CHANNELS_PER_DIMM = 235;
static constexpr auto SPD_JEDEC_DDR5_SUB_CHANNELS_PER_DIMM_MASK = 0x60;
static constexpr auto SPD_JEDEC_DDR5_PRI_BUS_WIDTH_PER_CHANNEL = 235;
static constexpr auto SPD_JEDEC_DDR5_PRI_BUS_WIDTH_PER_CHANNEL_MASK = 0x07;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_SYM_ALL = 6;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_ASYM_EVEN = 6;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_ASYM_ODD = 10;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_MASK = 0xE0;
static constexpr auto SPD_JEDEC_DDR5_DIE_PER_PKG_SYM_ALL = 4;
static constexpr auto SPD_JEDEC_DDR5_DIE_PER_PKG_ASYM_EVEN = 4;
static constexpr auto SPD_JEDEC_DDR5_DIE_PER_PKG_ASYM_ODD = 8;
static constexpr auto SPD_JEDEC_DDR5_DIE_PER_PKG_MASK = 0xE0;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_SYM_ALL = 4;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_ASYM_EVEN = 4;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_ASYM_ODD = 8;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_MASK = 0x1F;
static constexpr auto SPD_JEDEC_DDR5_RANK_MIX = 234;
static constexpr auto SPD_JEDEC_DDR5_RANK_MIX_SYMMETRICAL_MASK = 0x40;

auto isdimmVpdParser::getDDR4DimmCapacity(Binary::const_iterator& iterator)
{
    size_t tmp = 0, dimmSize = 0;

    size_t sdramCap = 1, priBusWid = 1, sdramWid = 1, logicalRanksPerDimm = 1;
    Byte dieCount = 1;

    // NOTE: This calculation is Only for DDR4

    // Calculate SDRAM  capacity
    tmp = iterator[constants::SPD_BYTE_4] & SPD_JEDEC_DDR4_SDRAM_CAP_MASK;
    /* Make sure the bits are not Reserved */
    if (tmp >= SPD_JEDEC_DDR4_SDRAMCAP_RESERVED)
    {
        std::cerr
            << "Bad data in spd byte 4. Can't calculate SDRAM capacity and so "
               "dimm size.\n ";
        return dimmSize;
    }

    sdramCap = (sdramCap << tmp) * SPD_JEDEC_DDR4_SDRAMCAP_MULTIPLIER;

    /* Calculate Primary bus width */
    tmp = iterator[constants::SPD_BYTE_13] & SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MASK;
    if (tmp >= SPD_JEDEC_DDR4_4_RESERVED_BITS)
    {
        std::cerr
            << "Bad data in spd byte 13. Can't calculate primary bus width "
               "and so dimm size.\n ";
        return dimmSize;
    }
    priBusWid = (priBusWid << tmp) * SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MULTIPLIER;

    /* Calculate SDRAM width */
    tmp = iterator[constants::SPD_BYTE_12] & SPD_JEDEC_DDR4_SDRAM_WIDTH_MASK;
    if (tmp >= SPD_JEDEC_DDR4_4_RESERVED_BITS)
    {
        std::cerr
            << "Bad data in vpd byte 12. Can't calculate SDRAM width and so "
               "dimm size.\n ";
        return dimmSize;
    }
    sdramWid = (sdramWid << tmp) * SPD_JEDEC_DDR4_SDRAM_WIDTH_MULTIPLIER;

    tmp = iterator[constants::SPD_BYTE_6] & SPD_JEDEC_DDR4_SIGNAL_LOADING_MASK;

    if (tmp == SPD_JEDEC_DDR4_SINGLE_LOAD_STACK)
    {
        // Fetch die count
        tmp = iterator[constants::SPD_BYTE_6] & SPD_JEDEC_DDR4_DIE_COUNT_MASK;
        tmp >>= SPD_JEDEC_DDR4_DIE_COUNT_RIGHT_SHIFT;
        dieCount = tmp + 1;
    }

    /* Calculate Number of ranks */
    tmp = iterator[constants::SPD_BYTE_12] & SPD_JEDEC_DDR4_NUM_RANKS_MASK;
    tmp >>= SPD_JEDEC_DDR4_3_RESERVED_BITS;

    if (tmp >= SPD_JEDEC_DDR4_4_RESERVED_BITS)
    {
        std::cerr << "Can't calculate number of ranks. Invalid data found.\n ";
        return dimmSize;
    }
    logicalRanksPerDimm = (tmp + 1) * dieCount;

    dimmSize = (sdramCap / SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MULTIPLIER) *
               (priBusWid / sdramWid) * logicalRanksPerDimm;

    return dimmSize;
}

auto isdimmVpdParser::getDDR4PartNumber(Binary::const_iterator& iterator)
{

    char tmpPN[constants::PART_NUM_LEN + 1] = {'\0'};
    sprintf(tmpPN, "%02X%02X%02X%X",
            iterator[SPD_JEDEC_DDR4_SDRAM_DENSITY_BANK_OFFSET],
            iterator[SPD_JEDEC_DDR4_SDRAM_ADDR_OFFSET],
            iterator[SPD_JEDEC_DDR4_DRAM_PRI_PACKAGE_OFFSET],
            iterator[SPD_JEDEC_DDR4_DRAM_MODULE_ORG_OFFSET] & 0x0F);
    std::string partNumber(tmpPN, sizeof(tmpPN));
    return partNumber;
}

auto isdimmVpdParser::getDDR4SerialNumber(Binary::const_iterator& iterator)
{
    char tmpSN[constants::SERIAL_NUM_LEN + 1] = {'\0'};
    sprintf(tmpSN, "%02X%02X%02X%02X%02X%02X",
            iterator[SPD_JEDEC_DDR4_MFG_ID_MSB_OFFSET],
            iterator[SPD_JEDEC_DDR4_MFG_ID_LSB_OFFSET],
            iterator[SPD_JEDEC_DDR4_SN_BYTE0_OFFSET],
            iterator[SPD_JEDEC_DDR4_SN_BYTE1_OFFSET],
            iterator[SPD_JEDEC_DDR4_SN_BYTE2_OFFSET],
            iterator[SPD_JEDEC_DDR4_SN_BYTE3_OFFSET]);
    std::string serialNumber(tmpSN, sizeof(tmpSN));
    return serialNumber;
}

auto isdimmVpdParser::getDDR4FruNumber(const std::string& partNumber)
{
    // check for 128GB ISRDIMM not implemented
    //(128GB 2RX4(8GX72) IS RDIMM 36*(16GBIT, 2H),1.2V 288PIN,1.2" ROHS) - NA

    static std::unordered_map<std::string, std::string> pnFruMap = {
        {"8421000", "78P4191"}, {"8421008", "78P4192"}, {"8529000", "78P4197"},
        {"8529008", "78P4198"}, {"8529928", "78P4199"}, {"8529B28", "78P4200"},
        {"8631008", "78P6815"}, {"8631928", "78P6925"}};

    std::string fruNumber;
    auto itr = pnFruMap.find(partNumber);
    if (itr != pnFruMap.end())
    {
        fruNumber = itr->second;
    }
    else
    {
        fruNumber = "FFFFFFF";
    }
    return fruNumber;
}

auto isdimmVpdParser::getDDR4CCIN(const std::string& partNumber)
{
    static std::unordered_map<std::string, std::string> pnCCINMap = {
        {"8421000", "324D"}, {"8421008", "324E"}, {"8529000", "324E"},
        {"8529008", "324F"}, {"8529928", "325A"}, {"8529B28", "324C"},
        {"8631008", "32BB"}, {"8631928", "32BC"}};

    std::string ccin;
    auto itr = pnCCINMap.find(partNumber);
    if (itr != pnCCINMap.end())
    {
        ccin = itr->second;
    }
    else
    {
        ccin = "XXXX";
    }
    return ccin;
}

auto isdimmVpdParser::getDDR5DimmCapacity(Binary::const_iterator& iterator)
{
    // dummy implementation to be updated when required
    size_t dimmSize = 0;
    (void)iterator;
    return dimmSize;
}

auto isdimmVpdParser::getDDR5PartNumber(Binary::const_iterator& iterator)
{
    // dummy implementation to be updated when required
    std::string partNumber;
    (void)iterator;
    partNumber = "0123456";
    return partNumber;
}

auto isdimmVpdParser::getDDR5SerialNumber(Binary::const_iterator& iterator)
{
    // dummy implementation to be updated when required
    std::string serialNumber;
    (void)iterator;
    serialNumber = "444444444444";
    return serialNumber;
}

auto isdimmVpdParser::getDDR5FruNumber(const std::string& partNumber)
{
    // dummy implementation to be updated when required
    static std::unordered_map<std::string, std::string> pnFruMap = {
        {"1234567", "XXXXXXX"}};

    std::string fruNumber;
    auto itr = pnFruMap.find(partNumber);
    if (itr != pnFruMap.end())
    {
        fruNumber = itr->second;
    }
    else
    {
        fruNumber = "FFFFFFF";
    }
    return fruNumber;
}

auto isdimmVpdParser::getDDR5CCIN(const std::string& partNumber)
{
    // dummy implementation to be updated when required
    static std::unordered_map<std::string, std::string> pnCCINMap = {
        {"1234567", "XXXX"}};

    std::string ccin;
    auto itr = pnCCINMap.find(partNumber);
    if (itr != pnCCINMap.end())
    {
        ccin = itr->second;
    }
    else
    {
        ccin = "XXXX";
    }
    return ccin;
}

kwdVpdMap isdimmVpdParser::readKeywords(Binary::const_iterator& iterator)
{
    inventory::KeywordVpdMap keywordValueMap{};
    if ((iterator[constants::SPD_BYTE_2] & constants::SPD_BYTE_MASK) ==
        constants::SPD_DRAM_TYPE_DDR5)
    {
        auto dimmSize = getDDR5DimmCapacity(iterator);
        if (!dimmSize)
        {
            std::cerr << "Error: Calculated dimm size is 0.";
        }
        else if (dimmSize < constants::CONVERT_MB_TO_KB)
        {
            keywordValueMap.emplace("MemorySizeInMB", dimmSize);
        }
        else
        {
            size_t dimmCapacityInGB = dimmSize / constants::CONVERT_MB_TO_KB;
            keywordValueMap.emplace("MemorySizeInGB", dimmCapacityInGB);
        }
        auto partNumber = getDDR5PartNumber(iterator);
        keywordValueMap.emplace("PN", move(partNumber));
        auto fruNumber = getDDR5FruNumber(partNumber);
        keywordValueMap.emplace("FN", move(fruNumber));
        auto serialNumber = getDDR5SerialNumber(iterator);
        keywordValueMap.emplace("SN", move(serialNumber));
        auto ccin = getDDR5CCIN(partNumber);
        keywordValueMap.emplace("CC", move(ccin));
    }
    else if ((iterator[constants::SPD_BYTE_2] & constants::SPD_BYTE_MASK) ==
             constants::SPD_DRAM_TYPE_DDR4)
    {
        auto dimmSize = getDDR4DimmCapacity(iterator);
        if (!dimmSize)
        {
            std::cerr << "Error: Calculated dimm size is 0.";
        }
        else if (dimmSize < constants::CONVERT_MB_TO_KB)
        {
            keywordValueMap.emplace("MemorySizeInMB", dimmSize);
        }
        else
        {
            size_t dimmCapacityInGB = dimmSize / constants::CONVERT_MB_TO_KB;
            keywordValueMap.emplace("MemorySizeInGB", dimmCapacityInGB);
        }
        size_t dimmCapacityInGB = dimmSize / constants::CONVERT_MB_TO_KB;
        keywordValueMap.emplace("MemorySizeInGB", dimmCapacityInGB);
        auto partNumber = getDDR4PartNumber(iterator);
        keywordValueMap.emplace("PN", move(partNumber));
        auto fruNumber = getDDR4FruNumber(partNumber);
        keywordValueMap.emplace("FN", move(fruNumber));
        auto serialNumber = getDDR4SerialNumber(iterator);
        keywordValueMap.emplace("SN", move(serialNumber));
        auto ccin = getDDR4CCIN(partNumber);
        keywordValueMap.emplace("CC", move(ccin));
    }
    return keywordValueMap;
}

std::variant<kwdVpdMap, Store> isdimmVpdParser::parse()
{
    // Read the data and return the map
    auto iterator = memVpd.cbegin();
    auto vpdDataMap = readKeywords(iterator);

    return vpdDataMap;
}

} // namespace parser
} // namespace isdimm
} // namespace vpd
} // namespace openpower
