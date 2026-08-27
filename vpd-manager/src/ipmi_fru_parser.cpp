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

std::expected<types::KWdVPDValueType, error_code> IpmiFruParser::decodeField(
    [[maybe_unused]] types::BinaryVector::const_iterator& i_pos,
    [[maybe_unused]] types::BinaryVector::const_iterator i_end) const noexcept
{
    /* @todo: implement decode field
     1. Check for area bounds
     2. Extract type code (8-bit ASCII or 6-bit ASCII)
     3. Extract the data bytes
    */
    return types::KWdVPDValueType{types::BinaryVector()};
}

std::expected<uint8_t, error_code> IpmiFruParser::parsePredefinedProductFields(
    types::BinaryVector::const_iterator& i_pos,
    types::BinaryVector::const_iterator i_end,
    types::IPMIVpdValueMap& o_map) noexcept
{
    // Ordered list of the seven predefined Product Info Area fields per
    // NVMe-MI spec §8.2.2 / IPMI FRU spec §12.
    const std::array<const char*, constants::VALUE_7> l_predefinedKeys{
        KW_PRODUCT_MNAME,       KW_PRODUCT_PNAME, KW_PRODUCT_PPMN,
        KW_PRODUCT_PVER,        KW_PRODUCT_PSN,   KW_PRODUCT_ASSET_TAG,
        KW_PRODUCT_FRU_FILE_ID,
    };

    for (const auto* l_key : l_predefinedKeys)
    {
        if (i_pos >= i_end)
        {
            // Ran out of data before reaching end-of-fields sentinel.
            m_logger->logMessage(std::format(
                "IPMI FRU: Product Info Area truncated while reading "
                "predefined field \"{}\" at offset {:#x}",
                l_key,
                static_cast<size_t>(
                    std::distance(m_vpdVector.cbegin(), i_pos))));
            return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
        }

        // Stop immediately if we hit the end-of-fields sentinel.
        if (*i_pos == END_OF_FIELDS)
        {
            break;
        }

        auto l_fieldResult = decodeField(i_pos, i_end);
        if (!l_fieldResult)
        {
            m_logger->logMessage(std::format(
                "IPMI FRU: Failed to decode Product Info Area field \"{}\" "
                "at offset {:#x}, error: {}",
                l_key,
                static_cast<size_t>(std::distance(m_vpdVector.cbegin(), i_pos)),
                commonUtility::getErrCodeMsg(l_fieldResult.error())));
            return std::unexpected(l_fieldResult.error());
        }
        o_map.emplace(l_key, std::move(*l_fieldResult));
    }

    return constants::VALUE_0;
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

std::expected<uint8_t, error_code> IpmiFruParser::parseProductInfoArea(
    const size_t i_offset) noexcept
{
    // ----------------------------------------------------------------
    // §8.2.2 Product Info Area layout (IPMI FRU spec §12 / NVMe-MI §8.2.2)
    //
    //   [0]  Format version  (bits 3:0 = 0x01)
    //   [1]  Product Info Area Length (PALEN) in multiples of 8 bytes
    //   [2]  Language code (LCODE)   (0x19 = English in NVMe-MI factory
    //   default) [3]  Manufacturer Name Type/Length (MNTL)
    //   ...  Manufacturer Name (MNAME)
    //   ...  Product Name Type/Length(PNTL)
    //   ...  Product Name (PNAME)
    //   ...  Product Part/Model Number Type/Length(PPMNNTL)
    //        Product Part/Model Number (PPMN)
    //   ...  Product Version Type/Length (PVTL)
    //   ...  Product Version (PVER)
    //   ...  Product Serial Number Type/Length (PSNTL)
    //   ...  Product Serial Number (PSN)
    //   ...  Asset Tag Type/Length (ATTL)
    //.  ...  Asset Tag (AT)
    //   ...  FRU File ID Type/Length (FFTL)
    //.  ...  FRU File ID (FFI)
    //.  ...  Custom Product Info Area (CPIA)
    //   ...  0xC1 End of Record (EOR) sentinel
    //   ...  0x00 padding to area boundary
    //   ...  Product Info Area checksum (PICHK) (zero-sum over the whole area)
    // ----------------------------------------------------------------
    // Minimum area is 8 bytes (1 unit): header + checksum with no fields.
    const size_t l_minAreaSize = AREA_OFFSET_MULTIPLIER;

    //  Validate format version byte (bits 3:0 must be 0x01).
    if ((m_vpdVector[i_offset] & 0x0FU) != IPMI_FRU_FORMAT_VERSION)
    {
        m_logger->logMessage(std::format(
            "IPMI FRU: Product Info Area format version mismatch at offset "
            "{:#x}, got {:#x}",
            i_offset, static_cast<unsigned>(m_vpdVector[i_offset] & 0x0FU)));
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    // Read the Product Info Area Length (PALEN) field (byte 1) and compute the
    // full area size.
    const size_t l_areaSize =
        static_cast<size_t>(m_vpdVector[i_offset + constants::VALUE_1]) *
        AREA_OFFSET_MULTIPLIER;

    if (l_areaSize < l_minAreaSize)
    {
        m_logger->logMessage(std::format(
            "IPMI FRU: Product Info Area length field is zero at offset "
            "{:#x}",
            i_offset));
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    // Validate if the area exceeds the buffer size
    if (i_offset + l_areaSize > m_vpdVector.size())
    {
        m_logger->logMessage(std::format(
            "IPMI FRU: Product Info Area (offset {:#x}, size {}) exceeds "
            "buffer size {}",
            i_offset, l_areaSize, m_vpdVector.size()));
        return std::unexpected(error_code::OUT_OF_BOUND_EXCEPTION);
    }

    // Validate the area checksum (zero-sum over the entire area including
    //    the checksum byte itself must equal 0x00).
    auto l_checksumResult = computeChecksum(
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset),
        m_vpdVector.cbegin() + static_cast<ptrdiff_t>(i_offset + l_areaSize));
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

    // Build the per-area value map
    types::IPMIVpdValueMap l_productInfoAreaMap;

    // Language code is a fixed single byte at offset +2 (no TL prefix)
    const uint8_t l_langCode = m_vpdVector[i_offset + constants::VALUE_2];
    l_productInfoAreaMap.emplace(KW_PRODUCT_LANGUAGE_CODE,
                                 types::BinaryVector{l_langCode});

    //  Walk variable-length fields starting at byte offset +3.
    //    The safe range ends one byte before the area checksum
    //    (last byte of the area), so the sentinel and all field data
    //    must fit inside [i_offset+3, i_offset+l_areaSize-1).
    auto l_pos = m_vpdVector.cbegin() +
                 static_cast<ptrdiff_t>(i_offset + constants::VALUE_3);

    // One-past-end of the field region (excludes the checksum byte).
    [[maybe_unused]] const auto l_areaFieldsEnd =
        m_vpdVector.cbegin() +
        static_cast<ptrdiff_t>(i_offset + l_areaSize - constants::VALUE_1);

    // Decode the seven predefined fields in order.
    auto l_predefinedResult = parsePredefinedProductFields(
        l_pos, l_areaFieldsEnd, l_productInfoAreaMap);
    if (!l_predefinedResult)
    {
        return std::unexpected(l_predefinedResult.error());
    }

    //@todo: parse any custom fields until the End Of Record sentinel.

    // Store the completed area map
    m_fruMap[static_cast<size_t>(types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)] =
        std::move(l_productInfoAreaMap);

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

    // Read area offsets
    const auto& l_areaOffsets = *l_headerStatus;
    const size_t l_productOffset = l_areaOffsets[static_cast<size_t>(
        types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)];

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

    // @todo: parse the other areas defined in IPMI FRU VPD format if they are
    // present

    return m_fruMap;
}

} // namespace vpd
