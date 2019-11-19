#include "ibm_vpd_type_check.hpp"

ibmVpdType ibmVpdTypeCheck(Binary ibmVpdVector)
{
    if (ibmVpdVector[11] == 0x84)
    {
        // IPZ VPD FORMAT
        return ibmVpdType::IPZ_VPD;
    }
    else if (ibmVpdVector[0] == 0x82)
    {
        // KEYWORD VPD FORMAT
        return ibmVpdType::Keyword_VPD;
    }

    // INVALID VPD FORMAT
    return (enum ::ibmVpdType) - 1;
}
