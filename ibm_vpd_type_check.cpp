#include "ibm_vpd_type_check.hpp"

#include "keyword_vpd_types.hpp"

using namespace vpd::keyword::parser;

namespace vpdFormat
{
ibmVpdType ibmVpdTypeCheck(const Binary& ibmVpdVector)
{
    if (ibmVpdVector[IPZ_DATA_START] == KW_VAL_PAIR_START_TAG)
    {
        // IPZ VPD FORMAT
        return ibmVpdType::IPZ_VPD;
    }
    else if (ibmVpdVector[KW_VPD_DATA_START] == KW_VPD_START_TAG)
    {
        // KEYWORD VPD FORMAT
        return ibmVpdType::KEYWORD_VPD;
    }

    // INVALID VPD FORMAT
    return ibmVpdType::INVALID_VPD_FORMAT;
}
} // namespace vpdFormat
