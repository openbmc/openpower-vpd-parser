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
    std::string partNumber(tmpPN, sizeof(tmpPN) - 1);
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
    std::string serialNumber(tmpSN, sizeof(tmpSN) - 1);
    return serialNumber;
}

auto isdimmVpdParser::getDDR4FruNumber(const std::string& partNumber,
                                       Binary::const_iterator& iterator)
{
    // check for 128GB ISRDIMM not implemented
    //(128GB 2RX4(8GX72) IS RDIMM 36*(16GBIT, 2H),1.2V 288PIN,1.2" ROHS) - NA

    // MTB Units is used in deciding the frequency of the DIMM
    // This is applicable only for DDR4 specification
    // 10 - DDR4-1600
    // 9  - DDR4-1866
    // 8  - DDR4-2133
    // 7  - DDR4-2400
    // 6  - DDR4-2666
    // 5  - DDR4-3200
    // pnFreqFnMap < tuple <partNumber, MTBUnits>, fruNumber>
    static std::map<std::tuple<std::string, uint8_t>, std::string> pnFreqFnMap =
        {{std::make_tuple("8421000", 6), "78P4191"},
         {std::make_tuple("8421008", 6), "78P4192"},
         {std::make_tuple("8529000", 6), "78P4197"},
         {std::make_tuple("8529008", 6), "78P4198"},
         {std::make_tuple("8529928", 6), "78P4199"},
         {std::make_tuple("8529B28", 6), "78P4200"},
         {std::make_tuple("8631928", 6), "78P6925"},
         {std::make_tuple("8529000", 5), "78P7317"},
         {std::make_tuple("8529008", 5), "78P7318"},
         {std::make_tuple("8631008", 5), "78P6815"}};

    std::string fruNumber;
    uint8_t mtbUnits = iterator[openpower::vpd::constants::SPD_BYTE_18] &
                       openpower::vpd::constants::SPD_BYTE_MASK;
    std::tuple<std::string, uint8_t> tup_key = {partNumber, mtbUnits};
    auto itr = pnFreqFnMap.find(tup_key);
    if (itr != pnFreqFnMap.end())
    {
        fruNumber = itr->second;
    }
    else
    {
        fruNumber = "FFFFFFF";
    }
    return fruNumber;
}

auto isdimmVpdParser::getDDR4CCIN(const std::string& fruNumber)
{
    static std::unordered_map<std::string, std::string> pnCCINMap = {
        {"78P4191", "324D"}, {"78P4192", "324E"}, {"78P4197", "324E"},
        {"78P4198", "324F"}, {"78P4199", "325A"}, {"78P4200", "324C"},
        {"78P6925", "32BC"}, {"78P7317", "331A"}, {"78P7318", "331F"},
        {"78P6815", "32BB"}};

    std::string ccin;
    auto itr = pnCCINMap.find(fruNumber);
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
        size_t dimmSize = getDDR5DimmCapacity(iterator);
        if (!dimmSize)
        {
            std::cerr << "Error: Calculated dimm size is 0.";
        }
        else
        {
            keywordValueMap.emplace("MemorySizeInKB", dimmSize);
        }
        auto partNumber = getDDR5PartNumber(iterator);
        auto fruNumber = getDDR5FruNumber(partNumber);
        keywordValueMap.emplace("FN", move(fruNumber));
        auto serialNumber = getDDR5SerialNumber(iterator);
        keywordValueMap.emplace("SN", move(serialNumber));
        auto ccin = getDDR5CCIN(partNumber);
        keywordValueMap.emplace("CC", move(ccin));
        keywordValueMap.emplace("PN", move(partNumber));
    }
    else if ((iterator[constants::SPD_BYTE_2] & constants::SPD_BYTE_MASK) ==
             constants::SPD_DRAM_TYPE_DDR4)
    {
        size_t dimmSize = getDDR4DimmCapacity(iterator);
        if (!dimmSize)
        {
            std::cerr << "Error: Calculated dimm size is 0.";
        }
        else
        {
            keywordValueMap.emplace("MemorySizeInKB",
                                    (dimmSize * constants::CONVERT_MB_TO_KB));
        }

        auto partNumber = getDDR4PartNumber(iterator);
        auto fruNumber = getDDR4FruNumber(partNumber, iterator);
        auto serialNumber = getDDR4SerialNumber(iterator);
        auto ccin = getDDR4CCIN(fruNumber);
        // PN value is made same as FN value
        auto displayPartNumber = fruNumber;
        keywordValueMap.emplace("PN", move(displayPartNumber));
        keywordValueMap.emplace("FN", move(fruNumber));
        keywordValueMap.emplace("SN", move(serialNumber));
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
