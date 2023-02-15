#include "memory_vpd_parser.hpp"

#include <iostream>
#include <numeric>
#include <string>

namespace openpower
{
namespace vpd
{
namespace memory
{
namespace parser
{
using namespace inventory;
using namespace constants;
using namespace std;
using namespace openpower::vpd::parser;

static constexpr auto MEM_BYTE_4 = 4;
static constexpr auto MEM_BYTE_6 = 6;
static constexpr auto MEM_BYTE_12 = 12;
static constexpr auto MEM_BYTE_13 = 13;
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
static constexpr auto JEDEC_SDRAMCAP_RESERVED = 6;
static constexpr auto JEDEC_RESERVED_BITS = 3;
static constexpr auto JEDEC_DIE_COUNT_RIGHT_SHIFT = 4;

static constexpr auto SPD_JEDEC_DDR5_SDRAM_NUM_OF_CHNLS_PER_DIMM_MASK = 0x60;
static constexpr auto SPD_JEDEC_DDR5_NUM_OF_PKG_RANK_PER_CHNL_MASK = 0x38;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_RANK_MIX_TYPE_MASK = 0x40;

static constexpr auto
    SPD_JEDEC_DDR5_NUM_OF_PKG_RANK_PER_CHNL_COUNT_RIGHT_SHIFT = 3;
static constexpr auto SPD_JEDEC_DDR5_NUM_OF_CHNLS_PER_DIMM_COUNT_RIGHT_SHIFT =
    5;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_COUNT_RIGHT_SHIFT = 5;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_DIE_PER_PKG_COUNT_RIGHT_SHIFT = 5;

static constexpr auto SPD_JEDEC_DDR5_SDRAM_RANK_MIX_TYPE_BIT_POSITION = 6;
static constexpr auto SPD_JEDEC_DDR5_NUM_OF_CHNL_PER_DIMM_RESERVED_BITS = 1;
static constexpr auto SPD_JEDEC_DDR5_PRI_BUS_WIDTH_PER_CHNL_RESERVED_BITS = 3;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_RESERVED_BITS = 3;
static constexpr auto SPD_JEDEC_DDR5_DIE_PER_PKG_RESERVED_BIT_001 = 1;
static constexpr auto SPD_JEDEC_DDR5_DIE_PER_PKG_RESERVED_BITS = 5;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_RESERVED_BITS = 8;
static constexpr auto SPD_JEDEC_DDR5_PRI_BUS_WIDTH_PER_CHANNEL_MASK = 0x07;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_MASK = 0x1F;
static constexpr auto SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_MASK = 0xE0;
static constexpr auto SPD_JEDEC_DDR5_DIE_PER_PKG_MASK = 0xE0;

static constexpr auto SPD_JEDEC_DDR5_PRI_BUS_WIDTH_PER_CHNL_MULTIPLIER = 8;

static const std::unordered_map<size_t, size_t> diePerPkgMap = {
    {0, 1}, {2, 2}, {3, 4}, {4, 8}, {5, 16}};

static const std::unordered_map<size_t, size_t> sdramDnstyPerDieMap = {
    {0, 0},  {1, 4},  {2, 8},  {3, 12}, {4, 16},
    {5, 24}, {6, 32}, {7, 48}, {8, 64}};

enum SdRamType
{
    EN_DDR4 = 0x0C,
    EN_DDR5 = 0x12
};

enum SdRamMixType
{
    EN_SYMMETRICAL = 0,
    EN_ASYMMETRICAL = 1
};

auto memoryVpdParser::getDimmSize(Binary::const_iterator iterator)
{
    size_t tmp = 0, dimmSize = 0;

    size_t sdramCap = 1, priBusWid = 1, sdramWid = 1, logicalRanksPerDimm = 1;
    Byte dieCount = 1;

    // NOTE: This calculation is Only for DDR4

    // Calculate SDRAM  capacity
    tmp = iterator[MEM_BYTE_4] & JEDEC_SDRAM_CAP_MASK;
    /* Make sure the bits are not Reserved */
    if (tmp > JEDEC_SDRAMCAP_RESERVED)
    {
        cerr << "Bad data in vpd byte 4. Can't calculate SDRAM capacity and so "
                "dimm size.\n ";
        return dimmSize;
    }

    sdramCap = (sdramCap << tmp) * JEDEC_SDRAMCAP_MULTIPLIER;

    /* Calculate Primary bus width */
    tmp = iterator[MEM_BYTE_13] & JEDEC_PRI_BUS_WIDTH_MASK;
    if (tmp > JEDEC_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 13. Can't calculate primary bus width "
                "and so dimm size.\n ";
        return dimmSize;
    }
    priBusWid = (priBusWid << tmp) * JEDEC_PRI_BUS_WIDTH_MULTIPLIER;

    /* Calculate SDRAM width */
    tmp = iterator[MEM_BYTE_12] & JEDEC_SDRAM_WIDTH_MASK;
    if (tmp > JEDEC_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 12. Can't calculate SDRAM width and so "
                "dimm size.\n ";
        return dimmSize;
    }
    sdramWid = (sdramWid << tmp) * JEDEC_SDRAM_WIDTH_MULTIPLIER;

    tmp = iterator[MEM_BYTE_6] & JEDEC_SIGNAL_LOADING_MASK;

    if (tmp == JEDEC_SINGLE_LOAD_STACK)
    {
        // Fetch die count
        tmp = iterator[MEM_BYTE_6] & JEDEC_DIE_COUNT_MASK;
        tmp >>= JEDEC_DIE_COUNT_RIGHT_SHIFT;
        dieCount = tmp + 1;
    }

    /* Calculate Number of ranks */
    tmp = iterator[MEM_BYTE_12] & JEDEC_NUM_RANKS_MASK;
    tmp >>= JEDEC_RESERVED_BITS;

    if (tmp > JEDEC_RESERVED_BITS)
    {
        cerr << "Can't calculate number of ranks. Invalid data found.\n ";
        return dimmSize;
    }
    logicalRanksPerDimm = (tmp + 1) * dieCount;

    dimmSize = (sdramCap / JEDEC_PRI_BUS_WIDTH_MULTIPLIER) *
               (priBusWid / sdramWid) * logicalRanksPerDimm;

    return dimmSize;
}

/**
 * @brief Find the count of even and odd numbers b/n 'num-1' range
 * where 'num' should be greater than zero.
 *
 * @param num[in] - The given number
 * @param numOfEvenNumbrs[out] - The even numbers count
 * @param numOfOddNumbrs[out] - The odd numbers count
 */
static void getNumOfEvenAndOddNumbrsInGivenNumRange(size_t& num,
                                                    size_t& numOfEvenNumbrs,
                                                    size_t& numOfOddNumbrs)
{
    if (num > 0)
    {
        num = num - 1;
        numOfOddNumbrs = num / 2;

        if (num % 2 != 0)
            numOfOddNumbrs += 1;

        numOfEvenNumbrs = (num + 1) - numOfOddNumbrs;
    }
}

/**
 * @brief Get the SDRAM parameters from the SPD for Symmetric Module &
 * Asymmetric Even Rank Module
 *
 * @param iterator[in] - iterator - iterator to buffer containing SPD
 * @param sdramIoWid[in,out] - The SDRAM I/O Width
 * @param diePerPkg[out] - The Die per Package
 * @param dramDnstyPerDie[out] - The SDRAM Density Per Die
 *
 * @return true if extracted sdram params are valid, false otherwise.
 */
static auto getSdramParamsFromSPDData(Binary::const_iterator& iterator,
                                      size_t& sdramIoWid, size_t& diePerPkg,
                                      size_t& sdramDnstyPerDie)
{
    bool retVal = true;
    size_t tmp = 0;

    // Calculate SDRAM I/O Width
    tmp = iterator[constants::SPD_BYTE_6] & SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_MASK;
    tmp = tmp >> SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_COUNT_RIGHT_SHIFT;
    if (tmp > SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 6 i.e " << tmp
             << ". So Can't calculate SDRAM i/o "
                "width with this bad data"
             << std::endl;

        retVal = false;
    }
    else
    {
        sdramIoWid = (sdramIoWid << tmp) * JEDEC_SDRAM_WIDTH_MULTIPLIER;
    }

    // Calculate Die per Package
    tmp = iterator[constants::SPD_BYTE_4] & SPD_JEDEC_DDR5_DIE_PER_PKG_MASK;
    tmp = tmp >> SPD_JEDEC_DDR5_SDRAM_DIE_PER_PKG_COUNT_RIGHT_SHIFT;

    if ((tmp == SPD_JEDEC_DDR5_DIE_PER_PKG_RESERVED_BIT_001) ||
        (tmp > SPD_JEDEC_DDR5_DIE_PER_PKG_RESERVED_BITS))
    {
        cerr << "Bad data in vpd byte 4 i.e " << tmp
             << ". So Can't calculate Die Per "
                "Package with this bad data"
             << std::endl;

        retVal = false;
    }
    else if (diePerPkgMap.find(tmp) != diePerPkgMap.end())
    {
        diePerPkg = diePerPkgMap.find(tmp)->second;
    }

    // Calculate SDRAM Density Per Die
    tmp = iterator[constants::SPD_BYTE_4] &
          SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_MASK;
    if (tmp > SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 4 i.e " << tmp
             << ". So Can't calculate the SDRAM Density "
                "Per Die with this bad data"
             << std::endl;

        retVal = false;
    }
    else if (sdramDnstyPerDieMap.find(tmp) != sdramDnstyPerDieMap.end())
    {
        sdramDnstyPerDie = sdramDnstyPerDieMap.find(tmp)->second;
    }

    return retVal;
}

/**
 * @brief Get the SDRAM parameters from the SPD for Asymmetric Module
 *
 * @param iterator[in] - iterator - iterator to buffer containing SPD
 * @param sdramIoWid[in,out] - The SDRAM I/O Width
 * @param diePerPkg[out] - The Die per Package
 * @param dramDnstyPerDie[out] - The SDRAM Density Per Die
 *
 * @return true if extracted sdram params are valid, false otherwise.
 */
static auto getSdramParamsForAsymmetricModule(Binary::const_iterator& iterator,
                                              size_t& sdramIoWid,
                                              size_t& diePerPkg,
                                              size_t& sdramDnstyPerDie)
{
    bool retVal = true;
    size_t tmp = 0;

    // Calculate Second SDRAM/Odd Ranks I/O Width
    tmp = iterator[constants::SPD_BYTE_10] & SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_MASK;

    tmp = tmp >> SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_COUNT_RIGHT_SHIFT;
    if (tmp > SPD_JEDEC_DDR5_SDRAM_IO_WIDTH_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 10. Can't calculate SDRAM "
                "i/o width of Asymmetrical odd ranks so "
                "dimm size.\n ";

        retVal = false;
    }
    else
    {
        sdramIoWid = (sdramIoWid << tmp) * JEDEC_SDRAM_WIDTH_MULTIPLIER;
    }

    // Calculate Die per Package
    tmp = iterator[constants::SPD_BYTE_8] & SPD_JEDEC_DDR5_DIE_PER_PKG_MASK;
    tmp = tmp >> SPD_JEDEC_DDR5_SDRAM_DIE_PER_PKG_COUNT_RIGHT_SHIFT;

    if ((tmp == SPD_JEDEC_DDR5_DIE_PER_PKG_RESERVED_BIT_001) ||
        (tmp > SPD_JEDEC_DDR5_DIE_PER_PKG_RESERVED_BITS))
    {
        cerr << "Bad data in vpd byte 8. Can't calculate Die Per "
                "Package of Asymmetrical odd ranks and so "
                "dimm size.\n ";

        retVal = false;
    }
    else if (diePerPkgMap.find(tmp) != diePerPkgMap.end())
    {
        diePerPkg = diePerPkgMap.find(tmp)->second;
    }

    // Calculate SDRAM Density Per Die
    tmp = iterator[constants::SPD_BYTE_8] &
          SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_MASK;

    if (tmp > SPD_JEDEC_DDR5_SDRAM_DENSITY_PER_DIE_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 8. Can't calculate SDRAM Density "
                "Per Die of Asymmetrical odd ranks and so "
                "dimm size.\n ";

        retVal = false;
    }
    else if (sdramDnstyPerDieMap.find(tmp) != sdramDnstyPerDieMap.end())
    {
        sdramDnstyPerDie = sdramDnstyPerDieMap.find(tmp)->second;
    }

    return retVal;
}

auto memoryVpdParser::getDDR5DimmSize(Binary::const_iterator& iterator)
{
    size_t tmp = 0, dimmSize = 0;

    size_t noOfChanlPerDimm = 0, priBusWidPerChanl = 1, noOfPkgRankPerChnl = 1;

    // Calculate number of channels per DIMM
    tmp = iterator[constants::SPD_BYTE_235] &
          SPD_JEDEC_DDR5_SDRAM_NUM_OF_CHNLS_PER_DIMM_MASK;

    tmp = tmp >> SPD_JEDEC_DDR5_NUM_OF_CHNLS_PER_DIMM_COUNT_RIGHT_SHIFT;

    if (tmp > SPD_JEDEC_DDR5_NUM_OF_CHNL_PER_DIMM_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 235. Can't calculate number of "
                "channel per DIMM so "
                "dimm size.\n ";

        return dimmSize;
    }
    noOfChanlPerDimm = tmp + 1;

    // Calculate Primary bus width per Channel
    tmp = iterator[constants::SPD_BYTE_235] &
          SPD_JEDEC_DDR5_PRI_BUS_WIDTH_PER_CHANNEL_MASK;

    if (tmp > SPD_JEDEC_DDR5_PRI_BUS_WIDTH_PER_CHNL_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 235. Can't calculate primary bus "
                "width "
                "and so dimm size.\n ";

        return dimmSize;
    }

    priBusWidPerChanl = (priBusWidPerChanl << tmp) *
                        SPD_JEDEC_DDR5_PRI_BUS_WIDTH_PER_CHNL_MULTIPLIER;

    // Calculate Number of Package Ranks per Channel
    tmp = iterator[constants::SPD_BYTE_234] &
          SPD_JEDEC_DDR5_NUM_OF_PKG_RANK_PER_CHNL_MASK;

    tmp = tmp >> SPD_JEDEC_DDR5_NUM_OF_PKG_RANK_PER_CHNL_COUNT_RIGHT_SHIFT;

    noOfPkgRankPerChnl += tmp;

    // Calculate Rank Mix Type(Symmetrical or Asymmetrical)
    size_t sdramRankMixType = SdRamMixType::EN_SYMMETRICAL;

    tmp = iterator[constants::SPD_BYTE_234] &
          SPD_JEDEC_DDR5_SDRAM_RANK_MIX_TYPE_MASK;

    // Checking for the specific position bit is set or not
    tmp &= (1 << SPD_JEDEC_DDR5_SDRAM_RANK_MIX_TYPE_BIT_POSITION);

    if (tmp)
    {
        sdramRankMixType = SdRamMixType::EN_ASYMMETRICAL;
    }

    size_t sdramIoWid = 1;
    size_t diePerPkg = 0, sdramDnstyPerDie = 0;

    bool retVal = getSdramParamsFromSPDData(iterator, sdramIoWid, diePerPkg,
                                            sdramDnstyPerDie);

    if (sdramRankMixType == SdRamMixType::EN_SYMMETRICAL)
    {
        if (!retVal)
        {
            // returning dimmSize as 0
            return dimmSize;
        }

        dimmSize = (noOfChanlPerDimm * (priBusWidPerChanl / sdramIoWid) *
                    diePerPkg * (sdramDnstyPerDie / 8) * noOfPkgRankPerChnl);
    }
    else
    {
        if (!retVal)
        {
            // returning dimmSize as 0
            return dimmSize;
        }

        size_t secSdramIoWid = 1;
        size_t secSdramDiePerPkg = 0, secSdramDnstyPerDie = 0;

        retVal = getSdramParamsForAsymmetricModule(
            iterator, secSdramIoWid, secSdramDiePerPkg, secSdramDnstyPerDie);

        if (!retVal)
        {
            // returning dimmSize as 0
            return dimmSize;
        }

        size_t noOfPkgRankPerChnlForPriSdRam = 0;
        size_t noOfPkgRankPerChnlForSecSdRam = 0;

        getNumOfEvenAndOddNumbrsInGivenNumRange(noOfPkgRankPerChnl,
                                                noOfPkgRankPerChnlForPriSdRam,
                                                noOfPkgRankPerChnlForSecSdRam);

        size_t capacityOfPrimSdRam =
            (noOfChanlPerDimm * (priBusWidPerChanl / sdramIoWid) * diePerPkg *
             (sdramDnstyPerDie / 8) * noOfPkgRankPerChnlForPriSdRam);

        size_t capacityOfSecSdram =
            (noOfChanlPerDimm * (priBusWidPerChanl / secSdramIoWid) *
             secSdramDiePerPkg * (secSdramDnstyPerDie / 8) *
             noOfPkgRankPerChnlForSecSdRam);

        dimmSize = capacityOfPrimSdRam + capacityOfSecSdram;
    }

    return dimmSize;
}

auto memoryVpdParser::getDDRXDimmSize(Binary::const_iterator& iterator)
{
    size_t dimmSize = 0;
    size_t sdramType = iterator[constants::SPD_BYTE_2];

    if (sdramType == SdRamType::EN_DDR4)
    {
        // collect Dimm size value
        dimmSize = getDimmSize(iterator);
    }
    else if (sdramType == SdRamType::EN_DDR5)
    {
        // collect DDR5 Dimm size value
        dimmSize = getDDR5DimmSize(iterator);
    }
    else
    {
        cerr << "Error: unknown SDRAM type: " << sdramType << endl;
    }
    return dimmSize;
}

kwdVpdMap memoryVpdParser::readKeywords(Binary::const_iterator iterator)
{
    KeywordVpdMap map{};

    size_t dimmSize = 0;

    // get the Dimm size based on the SDRAM types
    dimmSize = getDDRXDimmSize(iterator);

    if (!dimmSize)
    {
        cerr << "Error: Calculated dimm size is 0.";
    }

    map.emplace("MemorySizeInKB", dimmSize);
    // point the iterator to DIMM data and skip "11S"
    advance(iterator, MEMORY_VPD_DATA_START + 3);
    Binary partNumber(iterator, iterator + PART_NUM_LEN);

    advance(iterator, PART_NUM_LEN);
    Binary serialNumber(iterator, iterator + SERIAL_NUM_LEN);

    advance(iterator, SERIAL_NUM_LEN);
    Binary ccin(iterator, iterator + CCIN_LEN);

    map.emplace("FN", partNumber);
    map.emplace("PN", move(partNumber));
    map.emplace("SN", move(serialNumber));
    map.emplace("CC", move(ccin));

    return map;
}

variant<kwdVpdMap, Store> memoryVpdParser::parse()
{
    // Read the data and return the map
    auto iterator = memVpd.cbegin();
    auto vpdDataMap = readKeywords(iterator);

    return vpdDataMap;
}

std::string memoryVpdParser::getInterfaceName() const
{
    return memVpdInf;
}

} // namespace parser
} // namespace memory
} // namespace vpd
} // namespace openpower