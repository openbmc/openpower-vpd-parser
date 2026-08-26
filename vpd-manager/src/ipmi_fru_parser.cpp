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

namespace vpd
{

/* ========================================================================= */
/* Area parsers                                                               */
/* ========================================================================= */

std::expected<IpmiFruParser::AreaByteOffsets, error_code>
    IpmiFruParser::processCommonHeader() noexcept
{
    IpmiFruParser::AreaByteOffsets l_result{};

    // @todo:
    // parse the Common Header section
    // get individual area byte offsets by multiplying by 8
    // compute and validate checksum
    return l_result;
}

std::expected<uint8_t, error_code> IpmiFruParser::parseProductInfoArea(
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
