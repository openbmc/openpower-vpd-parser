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

auto memoryVpdParser::getDimmSize(const kwdVpdMap& vpdMap, size_t& tmp)
{
    size_t dimmSize = 0;
    size_t sdram_cap = 1, pri_bus_wid = 1, sdram_wid = 1,
           logical_ranks_per_dimm = 1;
    Byte SVPD_MEM_BYTE_4 = 0, SVPD_MEM_BYTE_6 = 0, SVPD_MEM_BYTE_12 = 0,
         SVPD_MEM_BYTE_13 = 0;

    for (auto& thispair : vpdMap)
    {
        if (thispair.first == "PN")
        {
            SVPD_MEM_BYTE_4 = thispair.second.at(1);
            SVPD_MEM_BYTE_6 = thispair.second.at(3);
        }
        else if (thispair.first == "SN")
        {
            SVPD_MEM_BYTE_12 = thispair.second.at(2);
            SVPD_MEM_BYTE_13 = thispair.second.at(3);
        }
    }

    Byte primaryBusWidthByteInSPD = SVPD_MEM_BYTE_13;
    Byte sdramCapacityByteInSPD = SVPD_MEM_BYTE_4;
    Byte sdramDeviceWidthByteInSPD = SVPD_MEM_BYTE_12;
    Byte packageRanksPerDimmByteInSPD = SVPD_MEM_BYTE_12;
    Byte dieCount = 1;
    size_t rc = 0;

    // NOTE: This calculation is Only for DDR4

    // Calculate SDRAM  capacity
    tmp = sdramCapacityByteInSPD & SVPD_JEDEC_SDRAM_CAP_MASK;
    /* Make sure the bits are not Reserved */
    if (tmp > SVPD_JEDEC_SDRAMCAP_RESERVED)
    {
        tmp = SVPD_MEM_BYTE_4;
        rc = -1;
        cout << "2nd- tmp- " << tmp << "\n";
        return rc;
    }
    cout << "sdramCapacityByteInSPD- " << sdramCapacityByteInSPD << "\n";
    cout << "SVPD_JEDEC_SDRAM_CAP_MASK- " << SVPD_JEDEC_SDRAM_CAP_MASK << "\n";
    cout << "SVPD_JEDEC_SDRAMCAP_RESERVED- " << SVPD_JEDEC_SDRAMCAP_RESERVED
         << "\n";
    cout << "tmp- " << tmp << "\n";
    cout << "SVPD_JEDEC_SDRAMCAP_MULTIPLIER- " << SVPD_JEDEC_SDRAMCAP_MULTIPLIER
         << "\n";
    cout << "sdram_cap- " << sdram_cap << "\n";

    sdram_cap = (sdram_cap << tmp) * SVPD_JEDEC_SDRAMCAP_MULTIPLIER;

    /* Calculate Primary bus width */
    tmp = primaryBusWidthByteInSPD & SVPD_JEDEC_PRI_BUS_WIDTH_MASK;
    if (tmp > SVPD_JEDEC_RESERVED_BITS)
    {
        tmp = SVPD_MEM_BYTE_13;
        rc = -2;
        return rc;
    }
    pri_bus_wid = (pri_bus_wid << tmp) * SVPD_JEDEC_PRI_BUS_WIDTH_MULTIPLIER;

    /* Calculate SDRAM width */
    tmp = sdramDeviceWidthByteInSPD & SVPD_JEDEC_SDRAM_WIDTH_MASK;
    if (tmp > SVPD_JEDEC_RESERVED_BITS)
    {
        tmp = SVPD_MEM_BYTE_12;
        rc = -3;
        return rc;
    }
    sdram_wid = (sdram_wid << tmp) * SVPD_JEDEC_SDRAM_WIDTH_MULTIPLIER;

    tmp = SVPD_MEM_BYTE_6 & SVPD_JEDEC_SIGNAL_LOADING_MASK;

    if (tmp == SVPD_JEDEC_SINGLE_LOAD_STACK)
    {
        // Fetch die count
        tmp = SVPD_MEM_BYTE_6 & SVPD_JEDEC_DIE_COUNT_MASK;
        tmp >>= SVPD_JEDEC_DIE_COUNT_RIGHT_SHIFT;
        dieCount = tmp + 1;
    }

    /* Calculate Number of ranks */
    tmp = packageRanksPerDimmByteInSPD & SVPD_JEDEC_NUM_RANKS_MASK;
    tmp >>= SVPD_JEDEC_RESERVED_BITS;

    if (tmp > SVPD_JEDEC_RESERVED_BITS)
    {
        tmp = SVPD_MEM_BYTE_12;
        rc = -4;
        return rc;
    }
    logical_ranks_per_dimm = (tmp + 1) * dieCount;

    dimmSize = (sdram_cap / SVPD_JEDEC_PRI_BUS_WIDTH_MULTIPLIER) *
               (pri_bus_wid / sdram_wid) * logical_ranks_per_dimm;
    tmp = 0;

    cout << "sdram_cap-" << sdram_cap << "\n";
    cout << "SVPD_JEDEC_PRI_BUS_WIDTH_MULTIPLIER- "
         << SVPD_JEDEC_PRI_BUS_WIDTH_MULTIPLIER << "\n";
    cout << "pri_bus_wid- " << pri_bus_wid << "\n";
    cout << "sdram_wid- " << sdram_wid << "\n";
    cout << "logical_ranks_per_dimm- " << logical_ranks_per_dimm << "\n";
    cout << "dimmSize- " << setfill('0') << setw(8) << hex << dimmSize << "\n";

    return dimmSize;
}

kwdVpdMap memoryVpdParser::readKeywords(Binary::const_iterator iterator)
{
    KeywordVpdMap map{};

    vector<uint8_t> partNumber(iterator, iterator + PART_NUM_LEN);

    advance(iterator, PART_NUM_LEN);
    vector<uint8_t> serialNumber(iterator, iterator + SERIAL_NUM_LEN);

    advance(iterator, SERIAL_NUM_LEN);
    vector<uint8_t> ccin(iterator, iterator + CCIN_LEN);

    map.emplace("PN", move(partNumber));
    map.emplace("SN", move(serialNumber));
    map.emplace("CC", move(ccin));

    size_t lError = 0;

    // colect Dimm size value
    auto value = getDimmSize(map, lError);
    if (lError)
    {
        throw runtime_error(
            "Couldn't calculate the size. One of the parameters has "
            "reserved value in the VPD. This BYTE has wrong value- " +
            lError);
    }

    cout << "getDimmSize Done-" << setfill('0') << setw(8) << hex << value
         << "\n";

    map.emplace("MemorySizeInKB", move(value));

    return map;
}

variant<kwdVpdMap, Store> memoryVpdParser::parse()
{
    // check if vpd file is empty
    if (memVpd.empty())
    {
        throw runtime_error("VPD file is empty.");
    }

    // Read the data and return the map
    auto iterator = memVpd.cbegin();
    // point the iterator to DIMM data and skip "11S"
    advance(iterator, MEMORY_VPD_DATA_START + 3);

    auto vpdDataMap = readKeywords(iterator);

    return vpdDataMap;
}

string memoryVpdParser::getInterfaceName() const
{
    return memVpdInf;
}

} // namespace parser
} // namespace memory
} // namespace vpd
} // namespace openpower
