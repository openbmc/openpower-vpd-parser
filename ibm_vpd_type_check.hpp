#pragma once
#include <types.hpp>

using namespace openpower::vpd;

/**
 * @brief Types of IBM VPD
 */
enum ibmVpdType
{
    IPZ_VPD,    /**< IPZ VPD type */
    Keyword_VPD /**< Keyword VPD type */
};

/**
 * @brief Check the type of IBM VPD.
 *
 * Checks the type of vpd based on the start tag.
 * @param[in] vector - Vpd data in vector format
 *
 * @return enum of type ibmVpdType
 */
ibmVpdType ibmVpdTypeCheck(Binary vector);
