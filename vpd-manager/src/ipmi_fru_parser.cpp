/**
 * @file ipmi_fru_parser.cpp
 *
 * IPMI Platform Management FRU Information Storage Definition v1.0 Rev 1.3
 * parser for the openpower-vpd-parser framework.
 *
 * Binary layout overview (all offsets are multiples of 8 bytes):
 *
 *  Offset  Size  Field
 *  ------  ----  -----
 *  0       8     Common Header
 *            [0]   Format version (bits 3:0 = 0x1)
 *            [1]   Internal Use Area offset  (0 = absent)
 *            [2]   Chassis Info Area offset  (0 = absent)
 *            [3]   Board Info Area offset    (0 = absent)
 *            [4]   Product Info Area offset  (0 = absent)
 *            [5]   MultiRecord Area offset   (0 = absent)
 *            [6]   PAD (0x00)
 *            [7]   Common Header Checksum (zero-sum over bytes 0-7)
 *
 * Each area (Chassis / Board / Product) starts with:
 *   [0]  Format version
 *   [1]  Area length in units of 8 bytes
 *   ...  fields  ...
 *   last byte: area checksum (zero-sum over the whole area)
 *
 * Each variable-length field is preceded by a type/length byte:
 *   bits 7:6  type code  (0=binary, 1=BCD+, 2=6-bit ASCII, 3=8-bit ASCII)
 *   bits 5:0  byte count of the data that follows
 *   0xC1      end-of-fields sentinel
 *
 * MultiRecord header (5 bytes per record):
 *   [0]  Record Type ID
 *   [1]  bit 7 = End-of-list; bits 3:0 = format version (=2)
 *   [2]  Record Length (bytes of data following the header)
 *   [3]  Record Checksum (zero-sum over record data bytes)
 *   [4]  Header Checksum (zero-sum over header bytes 0-4)
 */

#include "ipmi_fru_parser.hpp"

#include "constants.hpp"
#include "exceptions.hpp"

#include <format>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace vpd
{

/* ========================================================================= */
/* Internal helpers                                                           */
/* ========================================================================= */

void IpmiFruParser::validateChecksum(
    types::BinaryVector::const_iterator i_begin,
    types::BinaryVector::const_iterator i_end, uint8_t i_checkByte,
    const std::string& i_context) const
{
    // Zero-checksum: sum of all area bytes (including the checksum byte) mod
    // 256 must be 0.
    uint8_t l_sum =
        static_cast<uint8_t>(std::accumulate(i_begin, i_end, uint8_t{0}));
    l_sum = static_cast<uint8_t>(l_sum + i_checkByte);

    if (l_sum != 0)
    {
        throw DataException(
            std::format("IpmiFruParser: checksum validation failed for [{}]: "
                        "computed sum = 0x{:02X}",
                        i_context, l_sum));
    }
}

std::string IpmiFruParser::decode6BitAscii(
    types::BinaryVector::const_iterator i_data, size_t i_len) const
{
    // 6-bit ASCII: 4 characters packed into 3 bytes.
    // Character value v maps to ASCII (0x20 + v).
    // Packing (from spec section 13.3):
    //   byte0: char0[5:0]
    //   byte1: char1[3:0] | char0[5:4]<<4  -- actually:
    //
    //   bits layout per 3 bytes (b = byte index, c = char index):
    //   b0[5:0] = c0, b0[7:6] | b1[3:0] = c1, b1[7:4] | b2[1:0] = c2,
    //   b2[7:2] = c3
    //
    //   More precisely, the 4 characters are packed LSb first into 24 bits:
    //   bits  5:0  = char0
    //   bits 11:6  = char1
    //   bits 17:12 = char2
    //   bits 23:18 = char3

    std::string l_result;
    l_result.reserve((i_len / 3) * 4 + 4);

    for (size_t l_i = 0; l_i < i_len; l_i += 3)
    {
        // Gather 3 bytes; guard against running past end of data
        if (l_i + 3 > i_len)
        {
            // Fewer than 3 bytes remain — extract what we can.
            uint32_t l_bits = 0;
            size_t l_bytesLeft = i_len - l_i;
            for (size_t l_j = 0; l_j < l_bytesLeft; ++l_j)
            {
                l_bits |= static_cast<uint32_t>(*(i_data + l_i + l_j))
                          << (l_j * 8);
            }
            size_t l_charsLeft = (l_bytesLeft * 8 + 5) / 6;
            for (size_t l_c = 0; l_c < l_charsLeft; ++l_c)
            {
                l_result +=
                    static_cast<char>(0x20 + ((l_bits >> (l_c * 6)) & 0x3F));
            }
            break;
        }

        uint32_t l_bits = static_cast<uint32_t>(*(i_data + l_i)) |
                          (static_cast<uint32_t>(*(i_data + l_i + 1)) << 8) |
                          (static_cast<uint32_t>(*(i_data + l_i + 2)) << 16);

        l_result += static_cast<char>(0x20 + (l_bits & 0x3F));
        l_result += static_cast<char>(0x20 + ((l_bits >> 6) & 0x3F));
        l_result += static_cast<char>(0x20 + ((l_bits >> 12) & 0x3F));
        l_result += static_cast<char>(0x20 + ((l_bits >> 18) & 0x3F));
    }

    // Strip trailing spaces (common padding in 6-bit ASCII fields)
    while (!l_result.empty() && l_result.back() == ' ')
    {
        l_result.pop_back();
    }
    return l_result;
}

std::string IpmiFruParser::decodeField(
    uint8_t i_typelen, types::BinaryVector::const_iterator i_data,
    uint8_t i_langCode) const
{
    const uint8_t l_typeCode = static_cast<uint8_t>(
        (i_typelen & TYPELEN_TYPE_MASK) >> TYPELEN_TYPE_SHIFT);
    const size_t l_len = static_cast<size_t>(i_typelen & TYPELEN_LEN_MASK);

    if (l_len == 0)
    {
        return {};
    }

    switch (l_typeCode)
    {
        case TYPECODE_BINARY:
        {
            // Return hex representation
            std::ostringstream l_ss;
            for (size_t l_i = 0; l_i < l_len; ++l_i)
            {
                if (l_i > 0)
                {
                    l_ss << ' ';
                }
                l_ss << std::hex << std::uppercase << std::setfill('0')
                     << std::setw(2) << static_cast<unsigned>(*(i_data + l_i));
            }
            return l_ss.str();
        }

        case TYPECODE_BCD_PLUS:
        {
            // Each nibble encodes a digit/special character per spec 13.1
            static constexpr char BCD_MAP[] = "0123456789 -..";
            std::string l_out;
            l_out.reserve(l_len * 2);
            for (size_t l_i = 0; l_i < l_len; ++l_i)
            {
                const uint8_t l_byte = *(i_data + l_i);
                const uint8_t l_hi = (l_byte >> 4) & 0x0F;
                const uint8_t l_lo = l_byte & 0x0F;
                if (l_hi <= 0x0D)
                {
                    l_out += BCD_MAP[l_hi];
                }
                if (l_lo <= 0x0D)
                {
                    l_out += BCD_MAP[l_lo];
                }
            }
            return l_out;
        }

        case TYPECODE_6BIT_ASCII:
            return decode6BitAscii(i_data, l_len);

        case TYPECODE_8BIT_ASCII:
        default:
        {
            // When language code is English (0 or 25), type 11b = 8-bit
            // ASCII+Latin-1.  For non-English, it is 2-byte Unicode (LE).
            // We only handle 8-bit ASCII here; non-English Unicode is stored
            // as-is in the binary representation.
            const bool l_isEnglish = (i_langCode == 0 || i_langCode == 25);
            if (l_isEnglish)
            {
                return std::string(i_data, i_data + l_len);
            }
            else
            {
                // UTF-16 LE — return hex for now, consistent with binary type.
                std::ostringstream l_ss;
                for (size_t l_i = 0; l_i < l_len; ++l_i)
                {
                    if (l_i > 0)
                    {
                        l_ss << ' ';
                    }
                    l_ss << std::hex << std::uppercase << std::setfill('0')
                         << std::setw(2)
                         << static_cast<unsigned>(*(i_data + l_i));
                }
                return l_ss.str();
            }
        }
    }
}

std::optional<std::string> IpmiFruParser::readNextField(
    types::BinaryVector::const_iterator& io_it,
    types::BinaryVector::const_iterator i_areaEnd, uint8_t i_lang,
    size_t& o_offset, size_t& o_rawLen) const
{
    if (io_it >= i_areaEnd)
    {
        throw DataException(
            "IpmiFruParser: ran past area end while reading field type/length");
    }

    const uint8_t l_typelen = *io_it;

    if (l_typelen == FIELD_END_SENTINEL)
    {
        o_offset = 0;
        o_rawLen = 0;
        return std::nullopt; // end-of-fields
    }

    ++io_it; // consume type/length byte

    const size_t l_len = static_cast<size_t>(l_typelen & TYPELEN_LEN_MASK);

    if (io_it + static_cast<ptrdiff_t>(l_len) > i_areaEnd)
    {
        throw DataException(std::format(
            "IpmiFruParser: field length {} extends past area end", l_len));
    }

    o_offset = static_cast<size_t>(std::distance(m_vpdVector.cbegin(), io_it));
    o_rawLen = l_len;

    const std::string l_value = decodeField(l_typelen, io_it, i_lang);

    std::advance(io_it, static_cast<ptrdiff_t>(l_len));
    return l_value;
}

/* ========================================================================= */
/* Area parsers                                                               */
/* ========================================================================= */

void IpmiFruParser::parseInternalUseArea(size_t i_offset)
{
    // Minimum size: 1 (version byte) + at least 1 data byte.
    if (i_offset + 2 > m_vpdVector.size())
    {
        throw DataException("IpmiFruParser: Internal Use Area truncated");
    }

    // Validate format version (informational — area is opaque).
    const uint8_t l_version = m_vpdVector[i_offset];
    if ((l_version & 0x0F) != IPMI_FRU_FORMAT_VERSION)
    {
        m_logger->logMessage(
            std::format("IpmiFruParser: Internal Use Area version 0x{:02X} "
                        "is not 0x01 — proceeding anyway",
                        l_version));
    }

    // The area extent (and therefore the data to store) is only known by the
    // caller (parse()), which has access to all area offsets.  parse() will
    // populate m_fruMap["INTERNAL_Data"] and m_fieldOffsets directly after
    // calling this function.
}

void IpmiFruParser::parseChassisInfoArea(size_t i_offset)
{
    if (i_offset + 3 > m_vpdVector.size())
    {
        throw DataException("IpmiFruParser: Chassis Info Area truncated");
    }

    const size_t l_areaLen =
        static_cast<size_t>(m_vpdVector[i_offset + 1]) * AREA_UNIT_BYTES;

    if (i_offset + l_areaLen > m_vpdVector.size())
    {
        throw DataException(
            "IpmiFruParser: Chassis Info Area extends past buffer");
    }

    validateChecksum(
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset),
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + l_areaLen - 1),
        m_vpdVector[i_offset + l_areaLen - 1], "Chassis Info Area");

    // Byte [2]: chassis type enumeration
    const uint8_t l_chassisType = m_vpdVector[i_offset + 2];
    m_fruMap["CHASSIS_Type"] = types::BinaryVector{l_chassisType};

    auto l_it = m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + 3);
    const auto l_areaEnd =
        m_vpdVector.cbegin() +
        static_cast<ptrdiff_t>(i_offset + l_areaLen - 1); // last = checksum

    // Chassis Part Number and Serial Number are always English-encoded.
    const uint8_t l_lang = 0;

    const std::vector<std::string> l_predefined = {"CHASSIS_PartNumber",
                                                   "CHASSIS_SerialNumber"};

    for (const auto& l_fname : l_predefined)
    {
        size_t l_off = 0;
        size_t l_rawLen = 0;
        auto l_val = readNextField(l_it, l_areaEnd, l_lang, l_off, l_rawLen);
        if (!l_val)
        {
            break; // hit end-sentinel before all predefined fields
        }
        m_fruMap[l_fname] = types::BinaryVector(l_val->begin(), l_val->end());
        m_fieldOffsets[l_fname] = {l_off, l_rawLen};
        m_fieldAreaInfo[l_fname] = {i_offset, m_vpdVector[i_offset + 1]};
    }

    // Custom fields
    size_t l_customIdx = 1;
    while (l_it < l_areaEnd)
    {
        size_t l_off = 0;
        size_t l_rawLen = 0;
        auto l_val = readNextField(l_it, l_areaEnd, l_lang, l_off, l_rawLen);
        if (!l_val)
        {
            break;
        }
        const std::string l_key =
            "CHASSIS_Custom" + std::to_string(l_customIdx++);
        m_fruMap[l_key] = types::BinaryVector(l_val->begin(), l_val->end());
        m_fieldOffsets[l_key] = {l_off, l_rawLen};
        m_fieldAreaInfo[l_key] = {i_offset, m_vpdVector[i_offset + 1]};
    }
}

void IpmiFruParser::parseBoardInfoArea(size_t i_offset)
{
    if (i_offset + 7 > m_vpdVector.size())
    {
        throw DataException("IpmiFruParser: Board Info Area truncated");
    }

    const size_t l_areaLen =
        static_cast<size_t>(m_vpdVector[i_offset + 1]) * AREA_UNIT_BYTES;

    if (i_offset + l_areaLen > m_vpdVector.size())
    {
        throw DataException(
            "IpmiFruParser: Board Info Area extends past buffer");
    }

    validateChecksum(
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset),
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + l_areaLen - 1),
        m_vpdVector[i_offset + l_areaLen - 1], "Board Info Area");

    const uint8_t l_lang = m_vpdVector[i_offset + 2];
    m_fruMap["BOARD_LanguageCode"] = types::BinaryVector{l_lang};

    // Manufacturing date/time: 3 bytes, little-endian, minutes since
    // 1/1/1996 00:00.  Store as raw BinaryVector.
    const size_t l_mfgDateOffset = i_offset + 3;
    m_fruMap["BOARD_MfgDateTime"] = types::BinaryVector(
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(l_mfgDateOffset),
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(l_mfgDateOffset + 3));
    m_fieldOffsets["BOARD_MfgDateTime"] = {l_mfgDateOffset, 3};
    m_fieldAreaInfo["BOARD_MfgDateTime"] = {i_offset,
                                            m_vpdVector[i_offset + 1]};

    auto l_it = m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + 6);
    const auto l_areaEnd =
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + l_areaLen - 1);

    const std::vector<std::string> l_predefined = {
        "BOARD_Manufacturer", "BOARD_ProductName", "BOARD_SerialNumber",
        "BOARD_PartNumber", "BOARD_FruFileId"};

    for (const auto& l_fname : l_predefined)
    {
        size_t l_off = 0;
        size_t l_rawLen = 0;
        auto l_val = readNextField(l_it, l_areaEnd, l_lang, l_off, l_rawLen);
        if (!l_val)
        {
            break;
        }
        m_fruMap[l_fname] = types::BinaryVector(l_val->begin(), l_val->end());
        m_fieldOffsets[l_fname] = {l_off, l_rawLen};
        m_fieldAreaInfo[l_fname] = {i_offset, m_vpdVector[i_offset + 1]};
    }

    size_t l_customIdx = 1;
    while (l_it < l_areaEnd)
    {
        size_t l_off = 0;
        size_t l_rawLen = 0;
        auto l_val = readNextField(l_it, l_areaEnd, l_lang, l_off, l_rawLen);
        if (!l_val)
        {
            break;
        }
        const std::string l_key =
            "BOARD_Custom" + std::to_string(l_customIdx++);
        m_fruMap[l_key] = types::BinaryVector(l_val->begin(), l_val->end());
        m_fieldOffsets[l_key] = {l_off, l_rawLen};
        m_fieldAreaInfo[l_key] = {i_offset, m_vpdVector[i_offset + 1]};
    }
}

void IpmiFruParser::parseProductInfoArea(size_t i_offset)
{
    if (i_offset + 4 > m_vpdVector.size())
    {
        throw DataException("IpmiFruParser: Product Info Area truncated");
    }

    const size_t l_areaLen =
        static_cast<size_t>(m_vpdVector[i_offset + 1]) * AREA_UNIT_BYTES;

    if (i_offset + l_areaLen > m_vpdVector.size())
    {
        throw DataException(
            "IpmiFruParser: Product Info Area extends past buffer");
    }

    validateChecksum(
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset),
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + l_areaLen - 1),
        m_vpdVector[i_offset + l_areaLen - 1], "Product Info Area");

    const uint8_t l_lang = m_vpdVector[i_offset + 2];
    m_fruMap["PRODUCT_LanguageCode"] = types::BinaryVector{l_lang};

    auto l_it = m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + 3);
    const auto l_areaEnd =
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + l_areaLen - 1);

    const std::vector<std::string> l_predefined = {
        "PRODUCT_Manufacturer",    "PRODUCT_ProductName",
        "PRODUCT_PartModelNumber", "PRODUCT_Version",
        "PRODUCT_SerialNumber",    "PRODUCT_AssetTag",
        "PRODUCT_FruFileId"};

    for (const auto& l_fname : l_predefined)
    {
        size_t l_off = 0;
        size_t l_rawLen = 0;
        auto l_val = readNextField(l_it, l_areaEnd, l_lang, l_off, l_rawLen);
        if (!l_val)
        {
            break;
        }
        m_fruMap[l_fname] = types::BinaryVector(l_val->begin(), l_val->end());
        m_fieldOffsets[l_fname] = {l_off, l_rawLen};
        m_fieldAreaInfo[l_fname] = {i_offset, m_vpdVector[i_offset + 1]};
    }

    size_t l_customIdx = 1;
    while (l_it < l_areaEnd)
    {
        size_t l_off = 0;
        size_t l_rawLen = 0;
        auto l_val = readNextField(l_it, l_areaEnd, l_lang, l_off, l_rawLen);
        if (!l_val)
        {
            break;
        }
        const std::string l_key =
            "PRODUCT_Custom" + std::to_string(l_customIdx++);
        m_fruMap[l_key] = types::BinaryVector(l_val->begin(), l_val->end());
        m_fieldOffsets[l_key] = {l_off, l_rawLen};
        m_fieldAreaInfo[l_key] = {i_offset, m_vpdVector[i_offset + 1]};
    }
}

void IpmiFruParser::parseMultiRecordArea(size_t i_offset)
{
    size_t l_pos = i_offset;
    size_t l_recIdx = 1;

    while (l_pos + MULTIRECORD_HDR_SIZE <= m_vpdVector.size())
    {
        const uint8_t l_typeId = m_vpdVector[l_pos];
        const uint8_t l_flags = m_vpdVector[l_pos + 1];
        const uint8_t l_recLen = m_vpdVector[l_pos + 2];
        const uint8_t l_recChecksum = m_vpdVector[l_pos + 3];
        const uint8_t l_hdrChecksum = m_vpdVector[l_pos + 4];

        // Validate header checksum (bytes 0-4 must sum to 0)
        validateChecksum(
            m_vpdVector.cbegin() + static_cast<ptrdiff_t>(l_pos),
            m_vpdVector.cbegin() +
                static_cast<ptrdiff_t>(l_pos + MULTIRECORD_HDR_SIZE - 1),
            l_hdrChecksum, std::format("MultiRecord header {}", l_recIdx));

        const size_t l_dataStart = l_pos + MULTIRECORD_HDR_SIZE;

        if (l_dataStart + l_recLen > m_vpdVector.size())
        {
            throw DataException(
                std::format("IpmiFruParser: MultiRecord {} data extends past "
                            "buffer",
                            l_recIdx));
        }

        // Validate record data checksum
        if (l_recLen > 0)
        {
            validateChecksum(
                m_vpdVector.cbegin() + static_cast<ptrdiff_t>(l_dataStart),
                m_vpdVector.cbegin() +
                    static_cast<ptrdiff_t>(l_dataStart + l_recLen - 1),
                l_recChecksum, std::format("MultiRecord {} data", l_recIdx));
        }

        const std::string l_keyData = "MULTIRECORD_" + std::to_string(l_recIdx);
        const std::string l_keyType = l_keyData + "_TypeId";

        m_fruMap[l_keyType] = types::BinaryVector{l_typeId};
        m_fruMap[l_keyData] = types::BinaryVector(
            m_vpdVector.cbegin() + static_cast<ptrdiff_t>(l_dataStart),
            m_vpdVector.cbegin() +
                static_cast<ptrdiff_t>(l_dataStart + l_recLen));
        m_fieldOffsets[l_keyData] = {l_dataStart, l_recLen};
        m_fieldAreaInfo[l_keyData] = {l_pos, 0}; // no area-level checksum

        ++l_recIdx;
        l_pos = l_dataStart + l_recLen;

        if (l_flags & MULTIRECORD_EOL_BIT)
        {
            break; // End of list
        }
    }
}

/* ========================================================================= */
/* Checksum write-back                                                        */
/* ========================================================================= */

void IpmiFruParser::rewriteAreaChecksum(size_t i_areaStart, size_t i_areaLength)
{
    if (i_areaLength == 0)
    {
        return; // MultiRecord records use per-record checksums; skip.
    }

    const size_t l_areaBytes = i_areaLength * AREA_UNIT_BYTES;
    const size_t l_checksumOffset = i_areaStart + l_areaBytes - 1;

    if (l_checksumOffset >= m_vpdVector.size())
    {
        return;
    }

    // Compute 2's complement of sum of all bytes except the checksum byte.
    // m_vpdVector must already contain the updated field bytes at this point.
    uint8_t l_sum = std::accumulate(
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_areaStart),
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(l_checksumOffset),
        uint8_t{0});

    const uint8_t l_newChecksum = static_cast<uint8_t>(~l_sum + 1);

    // Update in-memory vector so future reads/checksums are consistent.
    m_vpdVector[l_checksumOffset] = l_newChecksum;

    if (m_vpdFileStream.is_open())
    {
        m_vpdFileStream.seekp(static_cast<std::streamoff>(l_checksumOffset),
                              std::ios::beg);
        m_vpdFileStream.write(reinterpret_cast<const char*>(&l_newChecksum), 1);
        m_vpdFileStream.flush();
    }
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

types::VPDMapVariant IpmiFruParser::parse()
{
    if (m_vpdVector.size() < COMMON_HEADER_SIZE)
    {
        throw DataException(
            std::format("IpmiFruParser: buffer too small ({} bytes) for "
                        "Common Header (8 bytes required)",
                        m_vpdVector.size()));
    }

    // Validate Common Header format version
    if ((m_vpdVector[0] & 0x0F) != IPMI_FRU_FORMAT_VERSION)
    {
        throw DataException(
            std::format("IpmiFruParser: unexpected Common Header format "
                        "version 0x{:02X}",
                        m_vpdVector[0] & 0x0F));
    }

    // Validate Common Header zero checksum (bytes 0-7 must sum to 0)
    validateChecksum(m_vpdVector.cbegin(),
                     m_vpdVector.cbegin() + COMMON_HEADER_SIZE - 1,
                     m_vpdVector[COMMON_HEADER_SIZE - 1], "Common Header");

    // Extract area offsets (0 = not present); spec offsets are in units of
    // 8 bytes.
    const size_t l_internalOffset =
        static_cast<size_t>(m_vpdVector[1]) * AREA_UNIT_BYTES;
    const size_t l_chassisOffset =
        static_cast<size_t>(m_vpdVector[2]) * AREA_UNIT_BYTES;
    const size_t l_boardOffset =
        static_cast<size_t>(m_vpdVector[3]) * AREA_UNIT_BYTES;
    const size_t l_productOffset =
        static_cast<size_t>(m_vpdVector[4]) * AREA_UNIT_BYTES;
    const size_t l_multirecOffset =
        static_cast<size_t>(m_vpdVector[5]) * AREA_UNIT_BYTES;

    // Parse each present area.  Order matches the spec layout (sec 17).
    if (l_internalOffset != 0)
    {
        try
        {
            parseInternalUseArea(l_internalOffset);

            // Determine the raw size of the Internal Use Area: it extends
            // to the start of the next non-zero area (or end of buffer).
            size_t l_nextOffset = m_vpdVector.size();
            for (size_t l_candidate : {l_chassisOffset, l_boardOffset,
                                       l_productOffset, l_multirecOffset})
            {
                if (l_candidate != 0 && l_candidate < l_nextOffset)
                {
                    l_nextOffset = l_candidate;
                }
            }
            const size_t l_dataSize = l_nextOffset - l_internalOffset - 1;
            const std::string l_key = "INTERNAL_Data";
            m_fruMap[l_key] = types::BinaryVector(
                m_vpdVector.cbegin() +
                    static_cast<ptrdiff_t>(l_internalOffset + 1),
                m_vpdVector.cbegin() +
                    static_cast<ptrdiff_t>(l_internalOffset + 1 + l_dataSize));
            m_fieldOffsets[l_key] = {l_internalOffset + 1, l_dataSize};
        }
        catch (const std::exception& l_ex)
        {
            m_logger->logMessage(
                std::format("IpmiFruParser: Internal Use Area parse error: {}",
                            l_ex.what()));
        }
    }

    if (l_chassisOffset != 0)
    {
        parseChassisInfoArea(l_chassisOffset);
    }

    if (l_boardOffset != 0)
    {
        parseBoardInfoArea(l_boardOffset);
    }

    if (l_productOffset != 0)
    {
        parseProductInfoArea(l_productOffset);
    }

    if (l_multirecOffset != 0)
    {
        parseMultiRecordArea(l_multirecOffset);
    }

    return m_fruMap;
}

types::DbusVariantType IpmiFruParser::readKeywordFromHardware(
    const types::ReadVpdParams i_params)
{
    const types::Keyword* l_keyword = std::get_if<types::Keyword>(&i_params);
    if (l_keyword == nullptr || l_keyword->empty())
    {
        throw types::DbusInvalidArgument();
    }

    // Ensure data is parsed
    if (m_fruMap.empty())
    {
        parse();
    }

    auto l_it = m_fruMap.find(*l_keyword);
    if (l_it == m_fruMap.end())
    {
        throw types::DbusInvalidArgument();
    }

    if (const auto* l_vec = std::get_if<types::BinaryVector>(&l_it->second))
    {
        return types::DbusVariantType{*l_vec};
    }

    if (const auto* l_str = std::get_if<std::string>(&l_it->second))
    {
        return types::DbusVariantType{
            types::BinaryVector(l_str->begin(), l_str->end())};
    }

    throw types::DbusInvalidArgument();
}

int IpmiFruParser::writeKeywordOnHardware(const types::WriteVpdParams i_params)
{
    types::Keyword l_keyword;
    types::BinaryVector l_value;

    if (const types::KwData* l_kw = std::get_if<types::KwData>(&i_params))
    {
        l_keyword = std::get<0>(*l_kw);
        l_value = std::get<1>(*l_kw);
    }
    else
    {
        throw types::DbusInvalidArgument();
    }

    if (l_keyword.empty())
    {
        throw DataException("IpmiFruParser: keyword name is empty");
    }

    if (l_value.empty())
    {
        throw DataException("IpmiFruParser: keyword value is empty");
    }

    if (m_vpdFilePath.empty())
    {
        throw DataException("IpmiFruParser: VPD file path not provided");
    }

    if (!m_vpdFileStream.is_open())
    {
        throw DataException(
            "IpmiFruParser: EEPROM file not open for write operations");
    }

    // Ensure we have parsed offset metadata.
    if (m_fieldOffsets.empty())
    {
        parse();
    }

    auto l_offIt = m_fieldOffsets.find(l_keyword);
    if (l_offIt == m_fieldOffsets.end())
    {
        throw DataException(
            std::format("IpmiFruParser: keyword [{}] not found", l_keyword));
    }

    const size_t l_byteOffset = l_offIt->second.first;
    const size_t l_capacity = l_offIt->second.second;

    if (l_capacity == 0)
    {
        throw DataException(
            std::format("IpmiFruParser: keyword [{}] has zero capacity — "
                        "cannot write",
                        l_keyword));
    }

    const size_t l_toWrite = std::min(l_value.size(), l_capacity);

    // 1. Update the in-memory vector first so checksum computation uses the
    //    new data (mirrors the pattern in
    //    KeywordVpdParser::writeKeywordOnHardware).
    std::copy_n(l_value.begin(), l_toWrite,
                m_vpdVector.begin() + static_cast<ptrdiff_t>(l_byteOffset));

    // 2. Recompute and apply the area checksum to both in-memory vector and
    // file.
    auto l_areaIt = m_fieldAreaInfo.find(l_keyword);
    if (l_areaIt != m_fieldAreaInfo.end() && l_areaIt->second.second != 0)
    {
        rewriteAreaChecksum(l_areaIt->second.first, l_areaIt->second.second);
    }

    // 3. Write the entire updated area region (field bytes + updated checksum)
    //    to the EEPROM file in one contiguous write when possible, otherwise
    //    write just the field bytes (checksum already written by
    //    rewriteAreaChecksum).
    m_vpdFileStream.seekp(static_cast<std::streamoff>(l_byteOffset),
                          std::ios::beg);
    m_vpdFileStream.write(reinterpret_cast<const char*>(l_value.data()),
                          static_cast<std::streamsize>(l_toWrite));
    m_vpdFileStream.flush();

    m_logger->logMessage(
        std::format("IpmiFruParser: {} bytes written for keyword [{}] at "
                    "offset 0x{:04X} in [{}]",
                    l_toWrite, l_keyword, l_byteOffset, m_vpdFilePath));

    return static_cast<int>(l_toWrite);
}

} // namespace vpd
