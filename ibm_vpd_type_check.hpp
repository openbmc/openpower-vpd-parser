#pragma once
#include <types.hpp>

using namespace openpower::vpd;

namespace vpdFormat
{
/**
 * @brief Types of VPD
 */
enum vpdType
{
    IPZ_VPD,           /**< IPZ VPD type */
    KEYWORD_VPD,       /**< Keyword VPD type */
    INVALID_VPD_FORMAT /**< Invalid VPD type */
};

/**
 * @brief Check the type of VPD.
 *
 * Checks the type of vpd based on the start tag.
 * @param[in] vector - Vpd data in vector format
 *
 * @return enum of type vpdType
 */
vpdType vpdTypeCheck(const Binary& vector);
} // namespace vpdFormat
