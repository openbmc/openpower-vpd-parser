#pragma once
#include "impl.hpp"

#include "vpdecc/vpdecc.h"

using namespace openpower::vpd::parser;
namespace openpower
{
namespace vpd
{
namespace modify
{
/** @brief This interface will create and write ECC for the given record
 *
 * @param[in] vpd - vpd in binary format
 * @param[in] iterator - Pointer pointing to the Offset of a record
 * @returns Success(0) OR Failed(-1)
 */
int createWriteEccForThisRecord(Binary& vpd, Binary::const_iterator iterator);

} // namespace modify
} // namespace vpd
} // namespace openpower
