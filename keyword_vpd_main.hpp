#ifndef KW_VPD_H
#define KW_VPD_H

#include <stdint.h>

#include <vector>

namespace openpower
{
namespace keywordVpd
{
namespace parser
{
#define KW_VPD_START_TAG 0x82
#define KW_VPD_END_TAG 0x78
#define KW_VAL_PAIR_START_TAG 0x84
#define KW_VAL_PAIR_END_TAG 0x79

using Binary = std::vector<uint8_t>;
} // namespace parser
} // namespace keywordVpd
} // namespace openpower
#endif
