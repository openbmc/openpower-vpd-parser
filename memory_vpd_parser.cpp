#include "memory_vpd_parser.hpp"

#include <iostream>
#include <numeric>
#include <string>

namespace vpd
{
namespace memory
{
namespace parser
{
using namespace openpower::vpd;
using namespace openpower::vpd::parser;
using namespace vpd::keyword::parser;
using namespace std;

KeywordVpdMap memoryVpdParser::readKeywords(Binary::const_iterator iterator)
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

    return map;
}

KeywordVpdMap memoryVpdParser::parseMemVpd()
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

} // namespace parser
} // namespace memory
} // namespace vpd
