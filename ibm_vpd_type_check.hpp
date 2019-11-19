#pragma once
#include <types.hpp>

using namespace openpower::vpd;

enum ibmVpdType
{
    IPZ_VPD,
    Keyword_VPD
};
ibmVpdType ibmVpdTypeCheck(Binary vector);
