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
        return static_cast<uint8_t>(
            std::accumulate(i_begin, i_end, static_cast<uint8_t>(0),
                            [](uint8_t l_acc, uint8_t l_byte) {
                                return static_cast<uint8_t>(l_acc + l_byte);
                            }));
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Error while computing checksum: {}", l_ex.what()));
        return std::unexpected(error_code::CHECKSUM_VALIDATION_FAILED);
    }
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
    if (!l_checksumResult || *l_checksumResult != constants::VALUE_0)
    {
        return std::unexpected(error_code::CHECKSUM_VALIDATION_FAILED);
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
    [[maybe_unused]] const size_t i_offset) noexcept
{
    // TODO: Validate area bounds and checksum.
    //       Populate: PRODUCT_LanguageCode, PRODUCT_Manufacturer,
    //       PRODUCT_ProductName, PRODUCT_PartModelNumber, PRODUCT_Version,
    //       PRODUCT_SerialNumber, PRODUCT_AssetTag, PRODUCT_FruFileId,
    //       and any PRODUCT_Custom<N> fields.
    //       Record offsets in m_fieldOffsets / m_fieldAreaInfo.
    return constants::VALUE_0;
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
        throw DataException(
            std::format("IPMI FRU: Common Header checksum mismatch. Error: {}",
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
