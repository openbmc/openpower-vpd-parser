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
#include "utility/common_utility.hpp"

#include <format>
#include <numeric>

namespace vpd
{

/* ========================================================================= */
/* Private helpers                                                            */
/* ========================================================================= */

std::expected<uint8_t, error_code> IpmiFruParser::computeChecksum(
    types::BinaryVector::const_iterator i_begin,
    types::BinaryVector::const_iterator i_end) const noexcept
{
    try
    {
        // sum all bytes mod 256
        const uint8_t l_sum =
            std::accumulate(i_begin, i_end, static_cast<uint8_t>(0),
                            [](uint8_t l_acc, uint8_t l_byte) {
                                return static_cast<uint8_t>(l_acc + l_byte);
                            });
        // return the two's complement
        return static_cast<uint8_t>(-l_sum);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Error while computing checksum: {}", l_ex.what()));
        return std::unexpected(error_code::INVALID_CHECKSUM_VALUE);
    }
}

/* ========================================================================= */
/* Field decoder                                                              */
/* ========================================================================= */

std::expected<types::KWdVPDValueType, error_code> IpmiFruParser::decodeField(
    types::BinaryVector::const_iterator& i_pos,
    types::BinaryVector::const_iterator i_end) const noexcept
{
    // Need at least one byte for the type/length byte itself.
    if (i_pos >= i_end)
    {
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    const uint8_t l_tl = *i_pos;
    ++i_pos;

    const uint8_t l_typeCode = static_cast<uint8_t>(l_tl & TL_TYPE_MASK);
    const uint8_t l_len = static_cast<uint8_t>(l_tl & TL_LENGTH_MASK);

    // Empty field (null placeholder): return an empty string.
    if (l_len == 0)
    {
        return types::KWdVPDValueType{std::string{}};
    }

    // Bounds check: make sure the declared data bytes are within range.
    if (static_cast<ptrdiff_t>(l_len) > std::distance(i_pos, i_end))
    {
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    const auto l_dataBegin = i_pos;
    const auto l_dataEnd = i_pos + static_cast<ptrdiff_t>(l_len);
    i_pos = l_dataEnd;

    if (l_typeCode == TL_TYPE_8BIT_ASCII)
    {
        // 8-bit ASCII / Latin-1: copy bytes directly to string.
        // NVMe-MI spec §8.2.2 Figure 151 notes that in this specification the
        // type code 11b always corresponds to ASCII.
        return types::KWdVPDValueType{
            std::string(l_dataBegin, l_dataEnd)};
    }

    if (l_typeCode == TL_TYPE_6BIT_ASCII)
    {
        // Packed 6-bit ASCII: 4 characters per 3 bytes, LS-char first.
        // Each 6-bit value maps to ASCII by adding 0x20 (space = 0).
        std::string l_str;
        l_str.reserve((l_len * 4U + 2U) / 3U);

        auto l_it = l_dataBegin;
        while (l_it < l_dataEnd)
        {
            // Consume up to 3 bytes, decode up to 4 characters.
            const size_t l_remaining =
                static_cast<size_t>(std::distance(l_it, l_dataEnd));
            const size_t l_chunk = std::min(l_remaining, static_cast<size_t>(3));

            // Load bytes (pad missing bytes with 0).
            const uint8_t l_b0 = *l_it;
            const uint8_t l_b1 =
                (l_chunk >= 2U) ? *(l_it + 1) : static_cast<uint8_t>(0);
            const uint8_t l_b2 =
                (l_chunk >= 3U) ? *(l_it + 2) : static_cast<uint8_t>(0);
            l_it += static_cast<ptrdiff_t>(l_chunk);

            // Extract four 6-bit values from the three bytes.
            // Byte layout: char0[5:0]=b0[5:0], char1[5:0]=b1[1:0]|b0[7:6],
            //              char2[5:0]=b2[3:0]|b1[7:2], char3[5:0]=b2[7:4]
            const uint32_t l_packed =
                (static_cast<uint32_t>(l_b2) << 16U) |
                (static_cast<uint32_t>(l_b1) << 8U) |
                static_cast<uint32_t>(l_b0);

            const size_t l_numChars =
                (l_chunk == 3U) ? 4U :
                (l_chunk == 2U) ? 3U : 2U;

            for (size_t l_i = 0; l_i < l_numChars; ++l_i)
            {
                const uint8_t l_sixBit =
                    static_cast<uint8_t>((l_packed >> (6U * l_i)) & 0x3FU);
                l_str += static_cast<char>(l_sixBit + 0x20U);
            }
        }
        return types::KWdVPDValueType{std::move(l_str)};
    }

    // TL_TYPE_BINARY and TL_TYPE_BCD_PLUS: return as raw bytes.
    return types::KWdVPDValueType{
        types::BinaryVector(l_dataBegin, l_dataEnd)};
}

/* ========================================================================= */
/* Area parsers                                                               */
/* ========================================================================= */

std::expected<IpmiFruParser::AreaByteOffsets, error_code>
    IpmiFruParser::processCommonHeader() noexcept
{
    // Validate the Common Header zero checksum: the modulo-256 sum of all 8
    // header bytes must be 0.
    auto l_checksumResult = computeChecksum(
        m_vpdVector.cbegin(),
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(COMMON_HEADER_SIZE));
    // error while computing checksum
    if (!l_checksumResult)
    {
        return std::unexpected(l_checksumResult.error());
    }
    // checksum value is not zero
    if (*l_checksumResult != constants::VALUE_0)
    {
        return std::unexpected(error_code::INVALID_CHECKSUM_VALUE);
    }

    IpmiFruParser::AreaByteOffsets l_result{};
    l_result[static_cast<size_t>(types::IpmiVpdAreaIndex::COMMON_HEADER)] =
        constants::VALUE_0;
    l_result[static_cast<size_t>(types::IpmiVpdAreaIndex::INTERNAL_USE_AREA)] =
        static_cast<size_t>(m_vpdVector[1]) * AREA_OFFSET_MULTIPLIER;
    l_result[static_cast<size_t>(types::IpmiVpdAreaIndex::CHASSIS_INFO_AREA)] =
        static_cast<size_t>(m_vpdVector[2]) * AREA_OFFSET_MULTIPLIER;
    l_result[static_cast<size_t>(types::IpmiVpdAreaIndex::BOARD_INFO_AREA)] =
        static_cast<size_t>(m_vpdVector[3]) * AREA_OFFSET_MULTIPLIER;
    l_result[static_cast<size_t>(types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)] =
        static_cast<size_t>(m_vpdVector[4]) * AREA_OFFSET_MULTIPLIER;
    l_result[static_cast<size_t>(types::IpmiVpdAreaIndex::MULTI_RECORD_AREA)] =
        static_cast<size_t>(m_vpdVector[5]) * AREA_OFFSET_MULTIPLIER;

    // Store the Common Header area map with the format-version byte.
    {
        types::IPMIVpdValueMap l_hdrMap;
        l_hdrMap.emplace(
            "HEADER_FormatVersion",
            types::KWdVPDValueType{types::BinaryVector{m_vpdVector[0]}});
        m_fruMap[static_cast<size_t>(types::IpmiVpdAreaIndex::COMMON_HEADER)] =
            std::move(l_hdrMap);
    }

    return l_result;
}

std::expected<size_t, error_code> IpmiFruParser::parseInternalUseArea(
    [[maybe_unused]] const size_t i_offset) noexcept
{
    // TODO: Validate the format version byte at m_vpdVector[i_offset].
    //       Log a warning (but do not throw) if the version is not 0x01.
    //       The raw data bytes (offset+1 onwards) are stored by parse()
    //       directly into m_fruMap["INTERNAL_Data"] after this call returns.
    return constants::VALUE_0;
}

std::expected<size_t, error_code> IpmiFruParser::parseChassisInfoArea(
    [[maybe_unused]] const size_t i_offset) noexcept
{
    // TODO: Validate area bounds and checksum.
    //       Populate: CHASSIS_Type (byte [2]), CHASSIS_PartNumber,
    //       CHASSIS_SerialNumber, and any CHASSIS_Custom<N> fields.
    //       Record field offsets and area info in m_fieldOffsets and
    //       m_fieldAreaInfo for later use by writeKeywordOnHardware().
    return constants::VALUE_0;
}

std::expected<size_t, error_code> IpmiFruParser::parseBoardInfoArea(
    [[maybe_unused]] const size_t i_offset) noexcept
{
    // TODO: Validate area bounds and checksum.
    //       Populate: BOARD_LanguageCode, BOARD_MfgDateTime (3 raw bytes),
    //       BOARD_Manufacturer, BOARD_ProductName, BOARD_SerialNumber,
    //       BOARD_PartNumber, BOARD_FruFileId, and any BOARD_Custom<N> fields.
    //       Record offsets in m_fieldOffsets / m_fieldAreaInfo.
    return constants::VALUE_0;
}

std::expected<size_t, error_code> IpmiFruParser::parseProductInfoArea(
    const size_t i_offset) noexcept
{
    // ----------------------------------------------------------------
    // §8.2.2 Product Info Area layout (IPMI FRU spec §12 / NVMe-MI §8.2.2)
    //
    //   [0]  Format version  (bits 3:0 = 0x01)
    //   [1]  Area length in multiples of 8 bytes
    //   [2]  Language code   (0x19 = English in NVMe-MI factory default)
    //   [3]  Manufacturer Name TL + N bytes  (MNAME)
    //   ...  Product Name TL + M bytes        (PNAME)
    //   ...  Part/Model Number TL + O bytes   (PPMN)
    //   ...  Product Version TL + R bytes     (PVER)
    //   ...  Serial Number TL + P bytes       (PSN)   [always ASCII]
    //   ...  Asset Tag TL + Q bytes           (ASSET_TAG)
    //   ...  FRU File ID TL + R bytes         (FRU_FILE_ID)
    //   ...  Custom fields (0..N), each preceded by a TL byte
    //   ...  0xC1 end-of-fields sentinel
    //   ...  0x00 padding to area boundary
    //   last Area checksum (zero-sum over the whole area)
    // ----------------------------------------------------------------

    // Minimum area is 8 bytes (1 unit): header + checksum with no fields.
    const size_t l_minAreaSize = AREA_OFFSET_MULTIPLIER;

    // 1. Validate that the area header itself is within buffer bounds.
    if (i_offset + l_minAreaSize > m_vpdVector.size())
    {
        m_logger->logMessage(std::format(
            "IPMI FRU: Product Info Area at offset {:#x} exceeds buffer "
            "size {}",
            i_offset, m_vpdVector.size()));
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    // 2. Read the area length field (byte 1) and compute the full area size.
    const size_t l_areaSize =
        static_cast<size_t>(m_vpdVector[i_offset + 1U]) *
        AREA_OFFSET_MULTIPLIER;

    if (l_areaSize < l_minAreaSize)
    {
        m_logger->logMessage(std::format(
            "IPMI FRU: Product Info Area length field is zero at offset "
            "{:#x}",
            i_offset));
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    if (i_offset + l_areaSize > m_vpdVector.size())
    {
        m_logger->logMessage(std::format(
            "IPMI FRU: Product Info Area (offset {:#x}, size {}) exceeds "
            "buffer size {}",
            i_offset, l_areaSize, m_vpdVector.size()));
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    // 3. Validate the area checksum (zero-sum over the entire area including
    //    the checksum byte itself must equal 0x00).
    auto l_checksumResult = computeChecksum(
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset),
        m_vpdVector.cbegin() +
            static_cast<ptrdiff_t>(i_offset + l_areaSize));
    if (!l_checksumResult)
    {
        return std::unexpected(l_checksumResult.error());
    }
    if (*l_checksumResult != constants::VALUE_0)
    {
        m_logger->logMessage(std::format(
            "IPMI FRU: Product Info Area checksum validation failed at "
            "offset {:#x}",
            i_offset));
        return std::unexpected(error_code::INVALID_CHECKSUM_VALUE);
    }

    // 4. Validate format version byte (bits 3:0 must be 0x01).
    if ((m_vpdVector[i_offset] & 0x0FU) != IPMI_FRU_FORMAT_VERSION)
    {
        m_logger->logMessage(std::format(
            "IPMI FRU: Product Info Area format version mismatch at offset "
            "{:#x}, got {:#x}",
            i_offset,
            static_cast<unsigned>(m_vpdVector[i_offset] & 0x0FU)));
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    // 5. Build the per-area value map.
    types::IPMIVpdValueMap l_areaMap;

    // Language code is a fixed single byte at offset +2 (no TL prefix).
    const uint8_t l_langCode = m_vpdVector[i_offset + 2U];
    l_areaMap.emplace(KW_PRODUCT_LANGUAGE_CODE,
                      types::KWdVPDValueType{types::BinaryVector{l_langCode}});

    // 6. Walk variable-length fields starting at byte offset +3.
    //    The safe range ends one byte before the area checksum
    //    (last byte of the area), so the sentinel and all field data
    //    must fit inside [i_offset+3, i_offset+l_areaSize-1).
    auto l_pos =
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + 3U);
    // One-past-end of the field region (excludes the checksum byte).
    const auto l_areaFieldsEnd =
        m_vpdVector.cbegin() +
        static_cast<ptrdiff_t>(i_offset + l_areaSize - 1U);

    // Ordered list of the seven predefined Product Info Area fields per
    // NVMe-MI spec §8.2.2 / IPMI FRU spec §12.
    const std::array<const char*, 7U> l_predefinedKeys{
        KW_PRODUCT_MNAME,
        KW_PRODUCT_PNAME,
        KW_PRODUCT_PPMN,
        KW_PRODUCT_PVER,
        KW_PRODUCT_PSN,
        KW_PRODUCT_ASSET_TAG,
        KW_PRODUCT_FRU_FILE_ID,
    };

    // Decode all seven mandatory predefined fields in order.
    for (const auto* l_key : l_predefinedKeys)
    {
        if (l_pos >= l_areaFieldsEnd)
        {
            // Ran out of data before reaching end-of-fields sentinel.
            m_logger->logMessage(std::format(
                "IPMI FRU: Product Info Area truncated while reading "
                "predefined field \"{}\" at offset {:#x}",
                l_key,
                static_cast<size_t>(
                    std::distance(m_vpdVector.cbegin(), l_pos))));
            return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
        }

        // Stop immediately if we hit the end-of-fields sentinel.
        if (*l_pos == END_OF_FIELDS)
        {
            break;
        }

        auto l_fieldResult = decodeField(l_pos, l_areaFieldsEnd);
        if (!l_fieldResult)
        {
            m_logger->logMessage(std::format(
                "IPMI FRU: Failed to decode Product Info Area field \"{}\" "
                "at offset {:#x}, error: {}",
                l_key,
                static_cast<size_t>(
                    std::distance(m_vpdVector.cbegin(), l_pos)),
                commonUtility::getErrCodeMsg(l_fieldResult.error())));
            return std::unexpected(l_fieldResult.error());
        }
        l_areaMap.emplace(l_key, std::move(*l_fieldResult));
    }

    // 7. Consume any custom fields until the end-of-fields sentinel.
    uint32_t l_customIdx = 0U;
    while (l_pos < l_areaFieldsEnd && *l_pos != END_OF_FIELDS)
    {
        auto l_fieldResult = decodeField(l_pos, l_areaFieldsEnd);
        if (!l_fieldResult)
        {
            m_logger->logMessage(std::format(
                "IPMI FRU: Failed to decode Product Info Area custom field "
                "{} at offset {:#x}, error: {}",
                l_customIdx,
                static_cast<size_t>(
                    std::distance(m_vpdVector.cbegin(), l_pos)),
                commonUtility::getErrCodeMsg(l_fieldResult.error())));
            return std::unexpected(l_fieldResult.error());
        }
        l_areaMap.emplace(
            std::format("CPIA_{}", l_customIdx),
            std::move(*l_fieldResult));
        ++l_customIdx;
    }

    // 8. Store the completed area map.
    m_fruMap[static_cast<size_t>(
        types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)] = std::move(l_areaMap);

    return l_areaSize;
}

std::expected<size_t, error_code> IpmiFruParser::parseMultiRecordArea(
    [[maybe_unused]] const size_t i_offset) noexcept
{
    // TODO: Walk the MultiRecord area starting at i_offset.
    //       For each record: validate both the header checksum and the record
    //       data checksum, then store data under "MULTIRECORD_<N>" and the
    //       type ID under "MULTIRECORD_<N>_TypeId".
    //       Stop when the End-of-list bit (bit 7 of header byte 1) is set.
    return constants::VALUE_0;
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

types::VPDMapVariant IpmiFruParser::parse()
{
    auto l_headerStatus = processCommonHeader();
    if (!l_headerStatus)
    {
        throw DataException(std::format(
            "IPMI FRU: Common Header checksum validation failed. Error: {}",
            commonUtility::getErrCodeMsg(l_headerStatus.error())));
    }

    const auto& l_areaOffsets = *l_headerStatus;
    const size_t l_internalUseOffset = l_areaOffsets[static_cast<size_t>(
        types::IpmiVpdAreaIndex::INTERNAL_USE_AREA)];
    const size_t l_chassisOffset = l_areaOffsets[static_cast<size_t>(
        types::IpmiVpdAreaIndex::CHASSIS_INFO_AREA)];
    const size_t l_boardOffset = l_areaOffsets[static_cast<size_t>(
        types::IpmiVpdAreaIndex::BOARD_INFO_AREA)];
    const size_t l_productOffset = l_areaOffsets[static_cast<size_t>(
        types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)];
    const size_t l_multirecordOffset = l_areaOffsets[static_cast<size_t>(
        types::IpmiVpdAreaIndex::MULTI_RECORD_AREA)];

    // Internal Use Area
    if (l_internalUseOffset != constants::VALUE_0)
    {
        auto l_status = parseInternalUseArea(l_internalUseOffset);
        if (!l_status)
        {
            m_logger->logMessage(
                std::format("Failed to parse Internal Use Area, error: {}",
                            commonUtility::getErrCodeMsg(l_status.error())));
        }
    }

    // Chassis Info Area
    if (l_chassisOffset != constants::VALUE_0)
    {
        auto l_status = parseChassisInfoArea(l_chassisOffset);
        if (!l_status)
        {
            m_logger->logMessage(
                std::format("Failed to parse Chassis Info Area, error: {}",
                            commonUtility::getErrCodeMsg(l_status.error())));
        }
    }

    // Board Info Area
    if (l_boardOffset != constants::VALUE_0)
    {
        auto l_status = parseBoardInfoArea(l_boardOffset);
        if (!l_status)
        {
            m_logger->logMessage(
                std::format("Failed to parse Board Info Area, error: {}",
                            commonUtility::getErrCodeMsg(l_status.error())));
        }
    }

    // Product Info Area
    if (l_productOffset != constants::VALUE_0)
    {
        auto l_status = parseProductInfoArea(l_productOffset);
        if (!l_status)
        {
            m_logger->logMessage(
                std::format("Failed to parse Product Info Area, error: {}",
                            commonUtility::getErrCodeMsg(l_status.error())));
        }
    }

    // MultiRecord Area
    if (l_multirecordOffset != constants::VALUE_0)
    {
        auto l_status = parseMultiRecordArea(l_multirecordOffset);
        if (!l_status)
        {
            m_logger->logMessage(
                std::format("Failed to parse MultiRecord Area, error: {}",
                            commonUtility::getErrCodeMsg(l_status.error())));
        }
    }

    return m_fruMap;
}

} // namespace vpd
