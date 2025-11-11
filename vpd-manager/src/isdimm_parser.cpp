#include "isdimm_parser.hpp"

#include "constants.hpp"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>
#include <unordered_map>

namespace vpd
{

// Constants
constexpr auto SPD_JEDEC_DDR4_SDRAM_CAP_MASK = 0x0F;
constexpr auto SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MASK = 0x07;
constexpr auto SPD_JEDEC_DDR4_SDRAM_WIDTH_MASK = 0x07;
constexpr auto SPD_JEDEC_DDR4_NUM_RANKS_MASK = 0x38;
constexpr auto SPD_JEDEC_DDR4_DIE_COUNT_MASK = 0x70;
constexpr auto SPD_JEDEC_DDR4_SINGLE_LOAD_STACK = 0x02;
constexpr auto SPD_JEDEC_DDR4_SIGNAL_LOADING_MASK = 0x03;

constexpr auto SPD_JEDEC_DDR4_SDRAMCAP_MULTIPLIER = 256;
constexpr auto SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MULTIPLIER = 8;
constexpr auto SPD_JEDEC_DDR4_SDRAM_WIDTH_MULTIPLIER = 4;
constexpr auto SPD_JEDEC_DDR4_SDRAMCAP_RESERVED = 8;
constexpr auto SPD_JEDEC_DDR4_4_RESERVED_BITS = 4;
constexpr auto SPD_JEDEC_DDR4_3_RESERVED_BITS = 3;
constexpr auto SPD_JEDEC_DDR4_DIE_COUNT_RIGHT_SHIFT = 4;

constexpr auto SPD_JEDEC_DDR4_MFG_ID_MSB_OFFSET = 321;
constexpr auto SPD_JEDEC_DDR4_MFG_ID_LSB_OFFSET = 320;
constexpr auto SPD_JEDEC_DDR4_SN_BYTE0_OFFSET = 325;
constexpr auto SPD_JEDEC_DDR4_SN_BYTE1_OFFSET = 326;
constexpr auto SPD_JEDEC_DDR4_SN_BYTE2_OFFSET = 327;
constexpr auto SPD_JEDEC_DDR4_SN_BYTE3_OFFSET = 328;
constexpr auto SPD_JEDEC_DDR4_SDRAM_DENSITY_BANK_OFFSET = 4;
constexpr auto SPD_JEDEC_DDR4_SDRAM_ADDR_OFFSET = 5;
constexpr auto SPD_JEDEC_DDR4_DRAM_PRI_PACKAGE_OFFSET = 6;
constexpr auto SPD_JEDEC_DDR4_DRAM_MODULE_ORG_OFFSET = 12;
static constexpr auto SPD_JEDEC_DDR4_DRAM_MANUFACTURER_ID_OFFSET = 320;
static constexpr auto SPD_JEDEC_DRAM_MANUFACTURER_ID_LENGTH = 2;

// Lookup tables
const std::map<std::tuple<std::string, uint8_t>, std::string> pnFreqFnMap = {
    {std::make_tuple("8421000", 6), "78P4191"},
    {std::make_tuple("8421008", 6), "78P4192"},
    {std::make_tuple("8529000", 6), "78P4197"},
    {std::make_tuple("8529008", 6), "78P4198"},
    {std::make_tuple("8529928", 6), "78P4199"},
    {std::make_tuple("8529B28", 6), "78P4200"},
    {std::make_tuple("8631928", 6), "78P6925"},
    {std::make_tuple("8529000", 5), "78P7317"},
    {std::make_tuple("8529008", 5), "78P7318"},
    {std::make_tuple("8631008", 5), "78P6815"}};

const std::unordered_map<std::string, std::string> pnCCINMap = {
    {"78P4191", "324D"}, {"78P4192", "324E"}, {"78P4197", "324E"},
    {"78P4198", "324F"}, {"78P4199", "325A"}, {"78P4200", "324C"},
    {"78P6925", "32BC"}, {"78P7317", "331A"}, {"78P7318", "331F"},
    {"78P6815", "32BB"}};

auto JedecSpdParser::getDDR4DimmCapacity(
    types::BinaryVector::const_iterator& i_iterator)
{
    size_t l_tmp = 0, l_dimmSize = 0;

    size_t l_sdramCap = 1, l_priBusWid = 1, l_sdramWid = 1,
           l_logicalRanksPerDimm = 1;
    size_t l_dieCount = 1;

    // NOTE: This calculation is Only for DDR4

    // Calculate SDRAM  capacity
    l_tmp = i_iterator[constants::SPD_BYTE_4] & SPD_JEDEC_DDR4_SDRAM_CAP_MASK;

    /* Make sure the bits are not Reserved */
    if (l_tmp >= SPD_JEDEC_DDR4_SDRAMCAP_RESERVED)
    {
        m_logger->logMessage(
            "Bad data in spd byte 4. Can't calculate SDRAM capacity "
            "and so dimm size.\n ",
            PlaceHolder::COLLECTION);
        return l_dimmSize;
    }
    l_sdramCap = (l_sdramCap << l_tmp) * SPD_JEDEC_DDR4_SDRAMCAP_MULTIPLIER;

    /* Calculate Primary bus width */
    l_tmp = i_iterator[constants::SPD_BYTE_13] &
            SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MASK;
    if (l_tmp >= SPD_JEDEC_DDR4_4_RESERVED_BITS)
    {
        m_logger->logMessage(
            "Bad data in spd byte 13. Can't calculate primary bus "
            "width and so dimm size.\n ",
            PlaceHolder::COLLECTION);
        return l_dimmSize;
    }
    l_priBusWid = (l_priBusWid << l_tmp) *
                  SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MULTIPLIER;

    /* Calculate SDRAM width */
    l_tmp = i_iterator[constants::SPD_BYTE_12] &
            SPD_JEDEC_DDR4_SDRAM_WIDTH_MASK;
    if (l_tmp >= SPD_JEDEC_DDR4_4_RESERVED_BITS)
    {
        m_logger->logMessage(
            "Bad data in spd byte 12. Can't calculate SDRAM width and "
            "so dimm size.\n ",
            PlaceHolder::COLLECTION);
        return l_dimmSize;
    }
    l_sdramWid = (l_sdramWid << l_tmp) * SPD_JEDEC_DDR4_SDRAM_WIDTH_MULTIPLIER;

    l_tmp = i_iterator[constants::SPD_BYTE_6] &
            SPD_JEDEC_DDR4_SIGNAL_LOADING_MASK;
    if (l_tmp == SPD_JEDEC_DDR4_SINGLE_LOAD_STACK)
    {
        // Fetch die count
        l_tmp = i_iterator[constants::SPD_BYTE_6] &
                SPD_JEDEC_DDR4_DIE_COUNT_MASK;
        l_tmp >>= SPD_JEDEC_DDR4_DIE_COUNT_RIGHT_SHIFT;
        l_dieCount = l_tmp + 1;
    }

    /* Calculate Number of ranks */
    l_tmp = i_iterator[constants::SPD_BYTE_12] & SPD_JEDEC_DDR4_NUM_RANKS_MASK;
    l_tmp >>= SPD_JEDEC_DDR4_3_RESERVED_BITS;

    if (l_tmp >= SPD_JEDEC_DDR4_4_RESERVED_BITS)
    {
        m_logger->logMessage(
            "Can't calculate number of ranks. Invalid data found.\n ",
            PlaceHolder::COLLECTION);
        return l_dimmSize;
    }
    l_logicalRanksPerDimm = (l_tmp + 1) * l_dieCount;

    l_dimmSize = (l_sdramCap / SPD_JEDEC_DDR4_PRI_BUS_WIDTH_MULTIPLIER) *
                 (l_priBusWid / l_sdramWid) * l_logicalRanksPerDimm;

    return l_dimmSize;
}

std::string_view JedecSpdParser::getDDR4PartNumber(
    types::BinaryVector::const_iterator& i_iterator)
{
    char l_tmpPN[constants::PART_NUM_LEN + 1] = {'\0'};
    sprintf(l_tmpPN, "%02X%02X%02X%X",
            i_iterator[SPD_JEDEC_DDR4_SDRAM_DENSITY_BANK_OFFSET],
            i_iterator[SPD_JEDEC_DDR4_SDRAM_ADDR_OFFSET],
            i_iterator[SPD_JEDEC_DDR4_DRAM_PRI_PACKAGE_OFFSET],
            i_iterator[SPD_JEDEC_DDR4_DRAM_MODULE_ORG_OFFSET] & 0x0F);
    std::string l_partNumber(l_tmpPN, sizeof(l_tmpPN) - 1);
    return l_partNumber;
}

std::string JedecSpdParser::getDDR4SerialNumber(
    types::BinaryVector::const_iterator& i_iterator)
{
    char l_tmpSN[constants::SERIAL_NUM_LEN + 1] = {'\0'};
    sprintf(l_tmpSN, "%02X%02X%02X%02X%02X%02X",
            i_iterator[SPD_JEDEC_DDR4_MFG_ID_MSB_OFFSET],
            i_iterator[SPD_JEDEC_DDR4_MFG_ID_LSB_OFFSET],
            i_iterator[SPD_JEDEC_DDR4_SN_BYTE0_OFFSET],
            i_iterator[SPD_JEDEC_DDR4_SN_BYTE1_OFFSET],
            i_iterator[SPD_JEDEC_DDR4_SN_BYTE2_OFFSET],
            i_iterator[SPD_JEDEC_DDR4_SN_BYTE3_OFFSET]);
    std::string l_serialNumber(l_tmpSN, sizeof(l_tmpSN) - 1);
    return l_serialNumber;
}

std::string_view JedecSpdParser::getDDR4FruNumber(
    const std::string& i_partNumber,
    types::BinaryVector::const_iterator& i_iterator)
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
    uint8_t l_mtbUnits = i_iterator[constants::SPD_BYTE_18] &
                         constants::SPD_BYTE_MASK;
    std::string l_fruNumber = "FFFFFFF";
    auto it = pnFreqFnMap.find({i_partNumber, l_mtbUnits});
    if (it != pnFreqFnMap.end())
    {
        l_fruNumber = it->second;
    }

    return l_fruNumber;
}

std::string_view JedecSpdParser::getDDR4CCIN(const std::string& i_fruNumber)
{
    auto it = pnCCINMap.find(i_fruNumber);
    if (it != pnCCINMap.end())
    {
        return it->second;
    }
    return "XXXX"; // Return default value as XXXX
}

types::BinaryVector JedecSpdParser::getDDR4ManufacturerId()
{
    types::BinaryVector l_mfgId(SPD_JEDEC_DRAM_MANUFACTURER_ID_LENGTH);

    if (m_memSpd.size() < (SPD_JEDEC_DDR4_DRAM_MANUFACTURER_ID_OFFSET +
                           SPD_JEDEC_DRAM_MANUFACTURER_ID_LENGTH))
    {
        m_logger->logMessage(
            "VPD length is less than the offset of Manufacturer ID. Can't fetch it",
            PlaceHolder::COLLECTION);
        return l_mfgId;
    }

    std::copy_n((m_memSpd.cbegin() +
                 SPD_JEDEC_DDR4_DRAM_MANUFACTURER_ID_OFFSET),
                SPD_JEDEC_DRAM_MANUFACTURER_ID_LENGTH, l_mfgId.begin());
    return l_mfgId;
}

auto JedecSpdParser::getDDR5DimmCapacity(
    types::BinaryVector::const_iterator& i_iterator)
{
    // dummy implementation to be updated when required
    size_t dimmSize = 0;
    (void)i_iterator;
    return dimmSize;
}

auto JedecSpdParser::getDDR5PartNumber(
    types::BinaryVector::const_iterator& i_iterator)
{
    // dummy implementation to be updated when required
    std::string l_partNumber;
    (void)i_iterator;
    l_partNumber = "0123456";
    return l_partNumber;
}

auto JedecSpdParser::getDDR5SerialNumber(
    types::BinaryVector::const_iterator& i_iterator)
{
    // dummy implementation to be updated when required
    std::string l_serialNumber;
    (void)i_iterator;
    l_serialNumber = "444444444444";
    return l_serialNumber;
}

auto JedecSpdParser::getDDR5FruNumber(const std::string& i_partNumber)
{
    // dummy implementation to be updated when required
    static std::unordered_map<std::string, std::string> pnFruMap = {
        {"1234567", "XXXXXXX"}};

    std::string l_fruNumber;
    auto itr = pnFruMap.find(i_partNumber);
    if (itr != pnFruMap.end())
    {
        l_fruNumber = itr->second;
    }
    else
    {
        l_fruNumber = "FFFFFFF";
    }
    return l_fruNumber;
}

auto JedecSpdParser::getDDR5CCIN(const std::string& i_partNumber)
{
    // dummy implementation to be updated when required
    static std::unordered_map<std::string, std::string> pnCCINMap = {
        {"1234567", "XXXX"}};

    std::string ccin = "XXXX";
    auto itr = pnCCINMap.find(i_partNumber);
    if (itr != pnCCINMap.end())
    {
        ccin = itr->second;
    }
    return ccin;
}

types::JedecSpdMap JedecSpdParser::readKeywords(
    types::BinaryVector::const_iterator& i_iterator)
{
    types::JedecSpdMap l_keywordValueMap{};
    size_t dimmSize = getDDR4DimmCapacity(i_iterator);
    if (!dimmSize)
    {
        m_logger->logMessage("Error: Calculated dimm size is 0.",
                             PlaceHolder::COLLECTION);
    }
    else
    {
        l_keywordValueMap.emplace("MemorySizeInKB",
                                  dimmSize * constants::CONVERT_MB_TO_KB);
    }

    auto l_partNumber = getDDR4PartNumber(i_iterator);
    auto l_fruNumber = getDDR4FruNumber(
        std::string(l_partNumber.begin(), l_partNumber.end()), i_iterator);
    auto l_serialNumber = getDDR4SerialNumber(i_iterator);
    auto ccin =
        getDDR4CCIN(std::string(l_fruNumber.begin(), l_fruNumber.end()));
    // PN value is made same as FN value
    auto l_displayPartNumber = l_fruNumber;
    auto l_mfgId = getDDR4ManufacturerId();

    l_keywordValueMap.emplace("PN",
                              move(std::string(l_displayPartNumber.begin(),
                                               l_displayPartNumber.end())));
    l_keywordValueMap.emplace(
        "FN", move(std::string(l_fruNumber.begin(), l_fruNumber.end())));
    l_keywordValueMap.emplace("SN", move(l_serialNumber));
    l_keywordValueMap.emplace("CC",
                              move(std::string(ccin.begin(), ccin.end())));
    l_keywordValueMap.emplace("DI", move(l_mfgId));

    return l_keywordValueMap;
}

types::VPDMapVariant JedecSpdParser::parse()
{
    // Read the data and return the map
    auto l_iterator = m_memSpd.cbegin();
    auto l_spdDataMap = readKeywords(l_iterator);
    return l_spdDataMap;
}

} // namespace vpd
