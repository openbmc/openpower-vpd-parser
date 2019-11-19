#include "ibm_vpd_type_check.hpp"

int ibmVpdTypeCheck(Binary ibmVpdVector)
{
    if (ibmVpdVector[11] == 0x84)
    {
        // IPZ VPD FORMAT
        return 0;
    }
    else if (ibmVpdVector[0] == 0x82)
    {
        // KEYWORD VPD FORMAT
        return 1;
    }

    // INVALID VPD FORMAT
    return -1;
}
