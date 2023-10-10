#include "memory_vpd_parser.hpp"

#include <cmath>
#include <cstdint>
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

static constexpr auto SDRAM_DENSITY_PER_DIE_24GB = 24;
static constexpr auto SDRAM_DENSITY_PER_DIE_32GB = 32;
static constexpr auto SDRAM_DENSITY_PER_DIE_48GB = 48;
static constexpr auto SDRAM_DENSITY_PER_DIE_64GB = 64;
static constexpr auto SDRAM_DENSITY_PER_DIE_UNDEFINED = 0;

static constexpr auto PRIMARY_BUS_WIDTH_32_BITS = 32;
static constexpr auto PRIMARY_BUS_WIDTH_UNUSED = 0;

bool memoryVpdParser::checkValidValue(uint8_t l_ByteValue, uint8_t shift,
                                      uint8_t minValue, uint8_t maxValue)
{
    l_ByteValue = l_ByteValue >> shift;
    if ((l_ByteValue > maxValue) || (l_ByteValue < minValue))
    {
        cout << "Non valid Value encountered value[" << l_ByteValue
             << "] range [" << minValue << ".." << maxValue << "] found "
             << " " << endl;
        return false;
    }
    else
    {
        return true;
    }
}

uint8_t memoryVpdParser::getDDR5DensityPerDie(uint8_t l_ByteValue)
{
    uint8_t l_densityPerDie = SDRAM_DENSITY_PER_DIE_UNDEFINED;
    if (l_ByteValue < constants::VALUE_5)
    {
        l_densityPerDie = l_ByteValue * constants::VALUE_4;
    }
    else
    {
        switch (l_ByteValue)
        {
            case VALUE_5:
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_24GB;
                break;

            case VALUE_6:
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_32GB;
                break;

            case VALUE_7:
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_48GB;
                break;

            case VALUE_8:
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_64GB;
                break;

            default:
                cout << "default value encountered for density per die" << endl;
                l_densityPerDie = SDRAM_DENSITY_PER_DIE_UNDEFINED;
                break;
        }
    }
    return l_densityPerDie;
}

uint8_t memoryVpdParser::getDDR5DiePerPackage(uint8_t l_ByteValue)
{
    uint8_t l_DiePerPackage = constants::VALUE_0;
    if (l_ByteValue < constants::VALUE_2)
    {
        l_DiePerPackage = l_ByteValue + constants::VALUE_1;
    }
    else
    {
        l_DiePerPackage = pow(constants::VALUE_2,
                              (l_ByteValue - constants::VALUE_1));
    }
    return l_DiePerPackage;
}

auto memoryVpdParser::getDdr5BasedDDimmSize(Binary::const_iterator iterator)
{
    size_t dimmSize = 0;

    do
    {
        if (!checkValidValue(iterator[constants::SPD_BYTE_235] &
                                 constants::MASK_BYTE_BITS_01,
                             constants::SHIFT_BITS_0, constants::VALUE_1,
                             constants::VALUE_3) ||
            !checkValidValue(iterator[constants::SPD_BYTE_235] &
                                 constants::MASK_BYTE_BITS_345,
                             constants::SHIFT_BITS_3, constants::VALUE_1,
                             constants::VALUE_3))
        {
            std::cerr
                << "Capacity calculation failed for channels per DIMM. DDIMM Byte 235 value ["
                << iterator[constants::SPD_BYTE_235] << "]";
            break;
        }
        uint8_t l_channelsPerDDimm =
            (((iterator[constants::SPD_BYTE_235] & constants::MASK_BYTE_BITS_01)
                  ? constants::VALUE_1
                  : constants::VALUE_0) +
             ((iterator[constants::SPD_BYTE_235] &
               constants::MASK_BYTE_BITS_345)
                  ? constants::VALUE_1
                  : constants::VALUE_0));

        if (!checkValidValue(iterator[constants::SPD_BYTE_235] &
                                 constants::MASK_BYTE_BITS_012,
                             constants::SHIFT_BITS_0, constants::VALUE_1,
                             constants::VALUE_3))
        {
            std::cerr
                << "Capacity calculation failed for bus width per channel. DDIMM Byte 235 value ["
                << iterator[constants::SPD_BYTE_235] << "]";
            break;
        }
        uint8_t l_busWidthPerChannel =
            (iterator[constants::SPD_BYTE_235] & constants::MASK_BYTE_BITS_012)
                ? PRIMARY_BUS_WIDTH_32_BITS
                : PRIMARY_BUS_WIDTH_UNUSED;

        if (!checkValidValue(iterator[constants::SPD_BYTE_4] &
                                 constants::MASK_BYTE_BITS_567,
                             constants::SHIFT_BITS_5, constants::VALUE_0,
                             constants::VALUE_5))
        {
            std::cerr
                << "Capacity calculation failed for die per package. DDIMM Byte 4 value ["
                << iterator[constants::SPD_BYTE_4] << "]";
            break;
        }
        uint8_t l_diePerPackage = getDDR5DiePerPackage(
            (iterator[constants::SPD_BYTE_4] & constants::MASK_BYTE_BITS_567) >>
            constants::VALUE_5);

        if (!checkValidValue(iterator[constants::SPD_BYTE_4] &
                                 constants::MASK_BYTE_BITS_01234,
                             constants::SHIFT_BITS_0, constants::VALUE_1,
                             constants::VALUE_8))
        {
            std::cerr
                << "Capacity calculation failed for SDRAM Density per Die. DDIMM Byte 4 value ["
                << iterator[constants::SPD_BYTE_4] << "]";
            break;
        }
        uint8_t l_densityPerDie = getDDR5DensityPerDie(
            iterator[constants::SPD_BYTE_4] & constants::MASK_BYTE_BITS_01234);

        uint8_t l_ranksPerChannel = ((iterator[constants::SPD_BYTE_234] &
                                      constants::MASK_BYTE_BITS_345) >>
                                     constants::VALUE_3) +
                                    (iterator[constants::SPD_BYTE_234] &
                                     constants::MASK_BYTE_BITS_012) +
                                    constants::VALUE_2;

        if (!checkValidValue(iterator[constants::SPD_BYTE_6] &
                                 constants::MASK_BYTE_BITS_567,
                             constants::SHIFT_BITS_5, constants::VALUE_0,
                             constants::VALUE_3))
        {
            std::cout
                << "Capacity calculation failed for dram width DDIMM Byte 6 value ["
                << iterator[constants::SPD_BYTE_6] << "]";
            break;
        }
        uint8_t l_dramWidth = VALUE_4 *
                              (VALUE_1 << ((iterator[constants::SPD_BYTE_6] &
                                            constants::MASK_BYTE_BITS_567) >>
                                           constants::VALUE_5));

        dimmSize = (l_channelsPerDDimm * l_busWidthPerChannel *
                    l_diePerPackage * l_densityPerDie * l_ranksPerChannel) /
                   (8 * l_dramWidth);

    } while (false);

    return constants::CONVERT_MB_TO_KB * dimmSize;
}

auto memoryVpdParser::getDdr4BasedDDimmSize(Binary::const_iterator iterator)
{
    size_t tmp = 0, dimmSize = 0;

    size_t sdramCap = 1, priBusWid = 1, sdramWid = 1, logicalRanksPerDimm = 1;
    Byte dieCount = 1;

    // NOTE: This calculation is Only for DDR4

    // Calculate SDRAM  capacity
    tmp = iterator[SPD_BYTE_4] & JEDEC_SDRAM_CAP_MASK;
    /* Make sure the bits are not Reserved */
    if (tmp > JEDEC_SDRAMCAP_RESERVED)
    {
        cerr << "Bad data in vpd byte 4. Can't calculate SDRAM capacity and so "
                "dimm size.\n ";
        return dimmSize;
    }

    sdramCap = (sdramCap << tmp) * JEDEC_SDRAMCAP_MULTIPLIER;

    /* Calculate Primary bus width */
    tmp = iterator[SPD_BYTE_13] & JEDEC_PRI_BUS_WIDTH_MASK;
    if (tmp > JEDEC_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 13. Can't calculate primary bus width "
                "and so dimm size.\n ";
        return dimmSize;
    }
    priBusWid = (priBusWid << tmp) * JEDEC_PRI_BUS_WIDTH_MULTIPLIER;

    /* Calculate SDRAM width */
    tmp = iterator[SPD_BYTE_12] & JEDEC_SDRAM_WIDTH_MASK;
    if (tmp > JEDEC_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 12. Can't calculate SDRAM width and so "
                "dimm size.\n ";
        return dimmSize;
    }
    sdramWid = (sdramWid << tmp) * JEDEC_SDRAM_WIDTH_MULTIPLIER;

    tmp = iterator[SPD_BYTE_6] & JEDEC_SIGNAL_LOADING_MASK;

    if (tmp == JEDEC_SINGLE_LOAD_STACK)
    {
        // Fetch die count
        tmp = iterator[SPD_BYTE_6] & JEDEC_DIE_COUNT_MASK;
        tmp >>= JEDEC_DIE_COUNT_RIGHT_SHIFT;
        dieCount = tmp + 1;
    }

    /* Calculate Number of ranks */
    tmp = iterator[SPD_BYTE_12] & JEDEC_NUM_RANKS_MASK;
    tmp >>= JEDEC_RESERVED_BITS;

    if (tmp > JEDEC_RESERVED_BITS)
    {
        cerr << "Can't calculate number of ranks. Invalid data found.\n ";
        return dimmSize;
    }
    logicalRanksPerDimm = (tmp + 1) * dieCount;

    dimmSize = (sdramCap / JEDEC_PRI_BUS_WIDTH_MULTIPLIER) *
               (priBusWid / sdramWid) * logicalRanksPerDimm;

    return constants::CONVERT_MB_TO_KB * dimmSize;
}

size_t memoryVpdParser::getDDimmSize(Binary::const_iterator iterator)
{
    size_t dimmSize = 0;
    if ((iterator[constants::SPD_BYTE_2] & constants::SPD_BYTE_MASK) ==
        constants::SPD_DRAM_TYPE_DDR4)
    {
        dimmSize = getDdr4BasedDDimmSize(iterator);
    }
    else if ((iterator[constants::SPD_BYTE_2] & constants::SPD_BYTE_MASK) ==
             constants::SPD_DRAM_TYPE_DDR5)
    {
        dimmSize = getDdr5BasedDDimmSize(iterator);
    }
    else
    {
        cerr << "Error: DDIMM is neither DDR4 nor DDR5. DDIMM Byte 2 value ["
             << iterator[constants::SPD_BYTE_2] << "]";
    }
    return dimmSize;
}

kwdVpdMap memoryVpdParser::readKeywords(Binary::const_iterator iterator)
{
    KeywordVpdMap map{};

    // collect Dimm size value
    auto dimmSize = getDDimmSize(iterator);
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
