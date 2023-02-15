#include "memory_vpd_parser.hpp"
#include "ibm_vpd_utils.hpp"

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

static constexpr auto MEM_BYTE_2 = 2;
static constexpr auto MEM_BYTE_4 = 4;
static constexpr auto MEM_BYTE_6 = 6;
static constexpr auto MEM_BYTE_8 = 8;
static constexpr auto MEM_BYTE_10 = 10;
static constexpr auto MEM_BYTE_12 = 12;
static constexpr auto MEM_BYTE_13 = 13;
static constexpr auto MEM_BYTE_234 = 234;
static constexpr auto MEM_BYTE_235 = 235;

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

static constexpr auto JEDEC_SDRAM_NUM_OF_CHNLS_PER_DIMM_MASK = 0x60;
static constexpr auto JEDEC_PRI_BUS_WIDTH_PER_CHNL_MASK = 0x07;
static constexpr auto JDEC_SDRAM_IO_WIDTH_MASK = 0xE0;
static constexpr auto JDEC_SDRAM_DIE_PER_PKG_MASK = 0xE0;
static constexpr auto JDEC_SDRAM_DENSITY_PER_DIE_MASK = 0x1F;
static constexpr auto JDEC_NUM_OF_PKG_RANK_PER_CHNL_MASK = 0x38;
static constexpr auto JDEC_SDRAM_RANK_MIX_TYPE_MASK = 0x40;

static constexpr auto JEDEC_NUM_OF_CHNLS_PER_DIMM_COUNT_RIGHT_SHIFT = 5;
static constexpr auto JEDEC_NUM_OF_PKG_RANK_PER_COUNT_RIGHT_SHIFT = 3;
static constexpr auto JEDEC_SDRAM_IO_WIDTH_COUNT_RIGHT_SHIFT = 5;
static constexpr auto JEDEC_SDRAM_DIE_PER_PKG_COUNT_RIGHT_SHIFT = 5;
static constexpr auto JEDEC_SDRAM_RANK_MIX_TYPE_BIT_POSITION = 6;

static constexpr auto JEDEC_NUM_OF_CHNL_PER_DIMM_RESERVED_VALUE = 1;
static constexpr auto JEDEC_PRI_BUS_WIDTH_PER_CHNL_RESERVED_VALUE = 3;
static constexpr auto JEDEC_NUM_OF_PKG_RANKS_PER_CHNL_RESERVED_VALUE = 7;
static constexpr auto JEDEC_SDRAM_IO_WIDTH_RESERVED_VALUE = 3;
static constexpr auto JEDEC_DIE_PER_PKG_RESERVED_VALUE1 = 1;
static constexpr auto JEDEC_DIE_PER_PKG_RESERVED_VALUE2 = 5;
static constexpr auto JEDEC_SDRAM_DENSITY_PER_DIE_RESERVED_VALUE = 8;

static constexpr auto JEDEC_PRI_BUS_WIDTH_PER_CHNL_MULTIPLIER = 8;

static const std::unordered_map<uint8_t, uint8_t> diePerPkgMap = {
    {0, 1}, {2, 2}, {3, 4}, {4, 8}, {5, 16}};

static const std::unordered_map<uint8_t, uint8_t> sdramDnstyPerDieMap = {
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

static void getNumOfEvenAndOddNumbrsInGivenNum(int& num,
                                               uint8_t& numOfEvenNumbrs,
                                               uint8_t& numOfOddNumbrs)

auto memoryVpdParser::getDimmSize(Binary::const_iterator iterator)
{
	size_t tmp = 0 size_t dimmSize = 0;

	size_t sdramCap = 1, priBusWid = 1, sdramWid = 1, logicalRanksPerDimm = 1;
	Byte dieCount = 1;

	// NOTE: This calculation is Only for DDR4

	// Calculate SDRAM  capacity
	tmp = iterator[MEM_BYTE_4] & JEDEC_SDRAM_CAP_MASK;
	/* Make sure the bits are not Reserved */
	if (tmp > JEDEC_SDRAMCAP_RESERVED)
	{
			cerr << "Bad data in vpd byte 4. Can't calculate SDRAM capacity "
					"and so "
					"dimm size.\n ";
			return dimmSize;
	}

	sdramCap = (sdramCap << tmp) * JEDEC_SDRAMCAP_MULTIPLIER;

	/* Calculate Primary bus width */
	tmp = iterator[MEM_BYTE_13] & JEDEC_PRI_BUS_WIDTH_MASK;
	if (tmp > JEDEC_RESERVED_BITS)
	{
			cerr
					<< "Bad data in vpd byte 13. Can't calculate primary bus width "
					"and so dimm size.\n ";
			return dimmSize;
	}
	priBusWid = (priBusWid << tmp) * JEDEC_PRI_BUS_WIDTH_MULTIPLIER;

	/* Calculate SDRAM width */
	tmp = iterator[MEM_BYTE_12] & JEDEC_SDRAM_WIDTH_MASK;
	if (tmp > JEDEC_RESERVED_BITS)
	{
			cerr << "Bad data in vpd byte 12. Can't calculate SDRAM width and "
					"so "
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

kwdVpdMap memoryVpdParser::readKeywords(Binary::const_iterator iterator)
{
    KeywordVpdMap map{};

    auto dimmSize = 0;
    uint8_t sdramType = iterator[MEM_BYTE_2];

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
        cerr << "Error: unknown SDRAM type.";
    }

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

/**
 * @brief Finds number of even and odd numbers in a given number range
 *
 * @param num[in] - The given number
 * @param numOfEvenNumbrs[out] - The number of even numbers count
 * @param numOfOddNumbrs[out] - The number of odd numbers count
 */
static void getNumOfEvenAndOddNumbrsInGivenNum(int& num,
                                               uint8_t& numOfEvenNumbrs,
                                               uint8_t& numOfOddNumbrs)
{
    uint8_t lowRange = 0;
    unit8_t highRange = num;

    numOfOddNumbrs = (lowRange - highRange) / 2;

    if (highRange % != 0 || lowRange % 2 != 0)
        evenNumCount += 1;

    numOfEvenNumbrs = (highRange - lowRange + 1) - numOfOddNumbrs;
}

auto memoryVpdParser::getDDR5DimmSize(Binary::const_iterator iterator)
{
    size_t tmp = 0, dimmSize = 0;

    uint8_t noOfChanlPerDimm = 1, priBusWidPerChanl = 1;
    int noOfPkgRankPerChnl = 1;

    // Calculate number of channels per DIMM
    tmp = iterator[MEM_BYTE_235] & JEDEC_SDRAM_NUM_OF_CHNLS_PER_DIMM_MASK;

    if (tmp > JEDEC_NUM_OF_CHNL_PER_DIMM_RESERVED_VALUE)
    {
        cerr << "Bad data in vpd byte 235. Can't calculate number of "
                "channel per DIMM so "
                "dimm size.\n ";
        return dimmSize;
    }
    noOfChanlPerDimm += tmp;

    // Calculate Primary bus width per Channel
    tmp = iterator[MEM_BYTE_235] & JEDEC_PRI_BUS_WIDTH_PER_CHNL_MASK;

    if (tmp > JEDEC_PRI_BUS_WIDTH_PER_CHNL_RESERVED_VALUE)
    {
        cerr << "Bad data in vpd byte 235. Can't calculate primary bus "
                "width "
                "and so dimm size.\n ";
        return dimmSize;
    }
    priBusWidPerChanl =
        (priBusWidPerChanl << tmp) * JEDEC_PRI_BUS_WIDTH_PER_CHNL_MULTIPLIER;

    // Calculate Number of Package Ranks per Channel
    tmp = iterator[MEM_BYTE_234] & JDEC_NUM_OF_PKG_RANK_PER_CHNL_MASK;
    tmp = tmp >> JEDEC_NUM_OF_PKG_RANK_PER_COUNT_RIGHT_SHIFT;
    if (tmp > JEDEC_NUM_OF_PKG_RANKS_PER_CHNL_RESERVED_VALUE)
    {
        cerr << "Bad data in vpd byte 234. Can't calculate number of "
                "package ranks per channel "
                "and so dimm size.\n ";
        return dimmSize;
    }
    noOfPkgRankPerChnl += tmp;

    // Calculate Rank Mix (Symmetrical or Asymmetrical)
    uint8_t sdramRankMixType = SdRamMixType::EN_SYMMETRICAL;
    tmp = iterator[MEM_BYTE_234] & JDEC_SDRAM_RANK_MIX_TYPE_MASK;

    tmp &= (1 << JEDEC_SDRAM_RANK_MIX_TYPE_BIT_POSITION);

    if (tmp == 1)
        sdramRankMixType = SdRamMixType::EN_ASYMMETRICAL;

    if (sdramRankMixType == SdRamMixType::EN_SYMMETRICAL)
    {
        size_t sdramIoWid = 1;
        uint8_t diePerPkg = 0, sdramDnstyPerDie = 0;

        // Calculate SDRAM I/O Width
        tmp = iterator[MEM_BYTE_6] & JDEC_SDRAM_IO_WIDTH_MASK;
        tmp = tmp >> JEDEC_SDRAM_IO_WIDTH_COUNT_RIGHT_SHIFT;
        if (tmp > JEDEC_SDRAM_IO_WIDTH_RESERVED_VALUE)
        {
            cerr << "Bad data in vpd byte 6. Can't calculate SDRAM i/o "
                    "width and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        sdramIoWid = (sdramIoWid << tmp) * JEDEC_SDRAM_WIDTH_MULTIPLIER;

        // Calculate Die per Package
        tmp = iterator[MEM_BYTE_4] & JDEC_SDRAM_DIE_PER_PKG_MASK;
        tmp = tmp >> JEDEC_SDRAM_DIE_PER_PKG_COUNT_RIGHT_SHIFT;

        if ((tmp == JEDEC_DIE_PER_PKG_RESERVED_VALUE1) ||
            (tmp > JEDEC_DIE_PER_PKG_RESERVED_VALUE2))
        {
            cerr << "Bad data in vpd byte 4. Can't calculate Die Per "
                    "Package and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        if (diePerPkgMap.find(tmp) != diePerPkgMap.end())
        {
            diePerPkg = diePerPkgMap.find(tmp)->second;
        }

        // Calculate SDRAM Density Per Die
        tmp = iterator[MEM_BYTE_4] & JDEC_SDRAM_DENSITY_PER_DIE_MASK;
        if (tmp > JEDEC_SDRAM_DENSITY_PER_DIE_RESERVED_VALUE)
        {
            cerr << "Bad data in vpd byte 4. Can't calculate SDRAM Density "
                    "Per Die and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        if (sdramDnstyPerDieMap.find(tmp) != sdramDnstyPerDieMap.end())
        {
            sdramDnstyPerDie = sdramDnstyPerDieMap.find(tmp)->second;
        }

        dimmSize = (noOfChanlPerDimm * priBusWidPerChanl) /
                   (sdramIoWid * diePerPkg * sdramDnstyPerDie) /
                   (8 * noOfPkgRankPerChnl);
    }
    else
    {
        size_t priSdramIoWid = 1;
        uint8_t priSdramDiePerPkg = 0, priSdramDnstyPerDie = 0;

        // Calculate First SDRAM I/O Width
        tmp = iterator[MEM_BYTE_6] & JDEC_SDRAM_IO_WIDTH_MASK;
        tmp = tmp >> JEDEC_SDRAM_IO_WIDTH_COUNT_RIGHT_SHIFT;
        if (tmp > JEDEC_SDRAM_IO_WIDTH_RESERVED_VALUE)
        {
            cerr << "Bad data in vpd byte 6. Can't calculate SDRAM i/o "
                    "width and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        priSdramIoWid = (priSdramIoWid << tmp) * JEDEC_SDRAM_WIDTH_MULTIPLIER;

        // Calculate Die per Package
        tmp = iterator[MEM_BYTE_4] & JDEC_SDRAM_DIE_PER_PKG_MASK;
        tmp = tmp >> JEDEC_SDRAM_DIE_PER_PKG_COUNT_RIGHT_SHIFT;

        if ((tmp == JEDEC_DIE_PER_PKG_RESERVED_VALUE1) ||
            (tmp > JEDEC_DIE_PER_PKG_RESERVED_VALUE2))
        {
            cerr << "Bad data in vpd byte 4. Can't calculate Die Per "
                    "Package and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        if (diePerPkgMap.find(tmp) != diePerPkgMap.end())
        {
            priSdramDiePerPkg = diePerPkgMap.find(tmp)->second;
        }

        // Calculate SDRAM Density Per Die
        tmp = iterator[MEM_BYTE_4] & JDEC_SDRAM_DENSITY_PER_DIE_MASK;
        if (tmp > JEDEC_SDRAM_DENSITY_PER_DIE_RESERVED_VALUE)
        {
            cerr << "Bad data in vpd byte 4. Can't calculate SDRAM Density "
                    "Per Die and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        if (sdramDnstyPerDieMap.find(tmp) != sdramDnstyPerDieMap.end())
        {
            priSdramDnstyPerDie = sdramDnstyPerDieMap.find(tmp)->second;
        }

        uint8_t evenRanksCount = 0;
        uint8_t oddRanksCount = 0;
        getNumOfEvenAndOddNumbrsInGivenNum(noOfPkgRankPerChnl, evenRanksCount,
                                           oddRanksCount);

        uint8_t noOfPkgRankPerChnlForPriSdRam = evenRanksCount;

        size_t capacityOfPrimSdRam =
            (noOfChanlPerDimm * priBusWidPerChanl) /
            (priSdramIoWid * priSdramDiePerPkg * priSdramDnstyPerDie) /
            (8 * noOfPkgRankPerChnlForPriSdRam);

        size_t secSdramIoWid = 1;
        uint8_t secSdramDiePerPkg = 0, secSdramDnstyPerDie = 0;

        // Calculate Second SDRAM I/O Width
        tmp = iterator[MEM_BYTE_10] & JDEC_SDRAM_IO_WIDTH_MASK;
        tmp = tmp >> JEDEC_SDRAM_IO_WIDTH_COUNT_RIGHT_SHIFT;
        if (tmp > JEDEC_SDRAM_IO_WIDTH_RESERVED_VALUE)
        {
            cerr << "Bad data in vpd byte 6. Can't calculate SDRAM i/o "
                    "width and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        secSdramIoWid = (secSdramIoWid << tmp) * JEDEC_SDRAM_WIDTH_MULTIPLIER;

        // Calculate Die per Package
        tmp = iterator[MEM_BYTE_8] & JDEC_SDRAM_DIE_PER_PKG_MASK;
        tmp = tmp >> JEDEC_SDRAM_DIE_PER_PKG_COUNT_RIGHT_SHIFT;

        if ((tmp == JEDEC_DIE_PER_PKG_RESERVED_VALUE1) ||
            (tmp > JEDEC_DIE_PER_PKG_RESERVED_VALUE2))
        {
            cerr << "Bad data in vpd byte 4. Can't calculate Die Per "
                    "Package and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        if (diePerPkgMap.find(tmp) != diePerPkgMap.end())
        {
            secSdramDiePerPkg = diePerPkgMap.find(tmp)->second;
        }

        // Calculate SDRAM Density Per Die
        tmp = iterator[MEM_BYTE_8] & JDEC_SDRAM_DENSITY_PER_DIE_MASK;
        if (tmp > JEDEC_SDRAM_DENSITY_PER_DIE_RESERVED_VALUE)
        {
            cerr << "Bad data in vpd byte 4. Can't calculate SDRAM Density "
                    "Per Die and so "
                    "dimm size.\n ";
            return dimmSize;
        }
        if (sdramDnstyPerDieMap.find(tmp) != sdramDnstyPerDieMap.end())
        {
            secSdramDnstyPerDie = sdramDnstyPerDieMap.find(tmp)->second;
        }
        uint8_t noOfPkgRankPerChnlForSecSdRam = oddRanksCount;

        size_t capacityOfSecSdram =
            (noOfChanlPerDimm * priBusWidPerChanl) /
            (secSdramIoWid * secSdramDiePerPkg * secSdramDnstyPerDie) /
            (8 * noOfPkgRankPerChnlForSecSdRam);

        dimmSize = capacityOfPrimSdRam + capacityOfSecSdram;
    }

    return dimmSize;
}
} // namespace parser
} // namespace memory
} // namespace vpd
} // namespace openpower