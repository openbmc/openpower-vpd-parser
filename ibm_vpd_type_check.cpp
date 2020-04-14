#include "ibm_vpd_type_check.hpp"

#include "keyword_vpd_types.hpp"
#include "memory_vpd_parser.hpp"

using namespace vpd::keyword::parser;
using namespace vpd::memory::parser;

namespace vpdFormat
{
vpdType vpdTypeCheck(const Binary& vpdVector)
{
    // Read first 3 Bytes to check the 11S bar code format
    std::string is11SFormat = "";
    for (uint8_t i = 0; i < FORMAT_11S_LEN; i++)
    {
        is11SFormat += vpdVector[MEMORY_VPD_DATA_START + i];
    }

    if (vpdVector[IPZ_DATA_START] == KW_VAL_PAIR_START_TAG)
    {
        // IPZ VPD FORMAT
        return vpdType::IPZ_VPD;
    }
    else if (vpdVector[KW_VPD_DATA_START] == KW_VPD_START_TAG)
    {
        // KEYWORD VPD FORMAT
        return vpdType::KEYWORD_VPD;
    }
    else if (is11SFormat.compare(MEMORY_VPD_START_TAG) == 0)
    {
        // Memory VPD format
        return vpdType::MEMORY_VPD;
    }

    // INVALID VPD FORMAT
    return vpdType::INVALID_VPD_FORMAT;
}
} // namespace vpdFormat
