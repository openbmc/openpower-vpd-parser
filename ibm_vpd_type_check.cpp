#include "ibm_vpd_type_check.hpp"

#include "keyword_vpd_types.hpp"

using namespace vpd::keyword::parser;

namespace vpdFormat
{
vpdType vpdTypeCheck(const Binary& vpdVector)
{
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

    // INVALID VPD FORMAT
    return vpdType::INVALID_VPD_FORMAT;
}
} // namespace vpdFormat
