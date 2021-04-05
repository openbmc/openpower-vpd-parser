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

auto memoryVpdParser::getDimmSize(Binary::const_iterator iterator)
{
    size_t tmp = 0, dimmSize = 0;

    size_t sdram_cap = 1, pri_bus_wid = 1, sdram_wid = 1,
           logical_ranks_per_dimm = 1;
    Byte dieCount = 1;

    // NOTE: This calculation is Only for DDR4

    // Calculate SDRAM  capacity
    tmp = iterator[SVPD_MEM_BYTE_4] & SVPD_JEDEC_SDRAM_CAP_MASK;
    /* Make sure the bits are not Reserved */
    if (tmp > SVPD_JEDEC_SDRAMCAP_RESERVED)
    {
        cerr << "Bad data in vpd byte 4. Can't calculate SDRAM capacity and so "
                "dimm size.\n ";
        return dimmSize;
    }

    sdram_cap = (sdram_cap << tmp) * SVPD_JEDEC_SDRAMCAP_MULTIPLIER;

    /* Calculate Primary bus width */
    tmp = iterator[SVPD_MEM_BYTE_13] & SVPD_JEDEC_PRI_BUS_WIDTH_MASK;
    if (tmp > SVPD_JEDEC_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 13. Can't calculate primary bus width "
                "and so dimm size.\n ";
        return dimmSize;
    }
    pri_bus_wid = (pri_bus_wid << tmp) * SVPD_JEDEC_PRI_BUS_WIDTH_MULTIPLIER;

    /* Calculate SDRAM width */
    tmp = iterator[SVPD_MEM_BYTE_12] & SVPD_JEDEC_SDRAM_WIDTH_MASK;
    if (tmp > SVPD_JEDEC_RESERVED_BITS)
    {
        cerr << "Bad data in vpd byte 12. Can't calculate SDRAM width and so "
                "dimm size.\n ";
        return dimmSize;
    }
    sdram_wid = (sdram_wid << tmp) * SVPD_JEDEC_SDRAM_WIDTH_MULTIPLIER;

    tmp = iterator[SVPD_MEM_BYTE_6] & SVPD_JEDEC_SIGNAL_LOADING_MASK;

    if (tmp == SVPD_JEDEC_SINGLE_LOAD_STACK)
    {
        // Fetch die count
        tmp = iterator[SVPD_MEM_BYTE_6] & SVPD_JEDEC_DIE_COUNT_MASK;
        tmp >>= SVPD_JEDEC_DIE_COUNT_RIGHT_SHIFT;
        dieCount = tmp + 1;
    }

    /* Calculate Number of ranks */
    tmp = iterator[SVPD_MEM_BYTE_12] & SVPD_JEDEC_NUM_RANKS_MASK;
    tmp >>= SVPD_JEDEC_RESERVED_BITS;

    if (tmp > SVPD_JEDEC_RESERVED_BITS)
    {
        cerr << "Can't calculate number of ranks. Invalid data found.\n ";
        return dimmSize;
    }
    logical_ranks_per_dimm = (tmp + 1) * dieCount;

    dimmSize = (sdram_cap / SVPD_JEDEC_PRI_BUS_WIDTH_MULTIPLIER) *
               (pri_bus_wid / sdram_wid) * logical_ranks_per_dimm;

    return dimmSize;
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