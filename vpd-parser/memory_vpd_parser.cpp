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

    return constants::CONVERT_MB_TO_KB * dimmSize;
}

kwdVpdMap memoryVpdParser::readKeywords(Binary::const_iterator iterator)
{
    KeywordVpdMap map{};

    // collect Dimm size value
    auto dimmSize = getDimmSize(iterator);
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
