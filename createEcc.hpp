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
 * @param[in] recordOffset - Offset of a record in the VPD
 * @param[in] recordLength - Length of record in the VPD
 * @param[in] eccOffset    - Offset of record's ECC
 * @param[in] eccLength    - Length of record's ECC
 * @returns Success(0) OR Failed(-1)
 */
int createWriteEccForThisRecord(Binary&& vpd, const LE2ByteData recordOffset,
                                LE2ByteData recordLength, LE2ByteData eccOffset,
                                LE2ByteData eccLength);

} // namespace modify
} // namespace vpd
} // namespace openpower
