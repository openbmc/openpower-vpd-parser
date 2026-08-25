#pragma once

#include "constants.hpp"
#include "error_codes.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

#include <array>
#include <expected>
#include <optional>

namespace vpd
{

/**
 * @brief Concrete class to implement IPMI FRU VPD parsing.
 *
 * Parses the binary FRU Information Storage format defined by the IPMI
 * Platform Management FRU Information Storage Definition v1.0, Rev 1.3.
 *
 * The format consists of up to six areas, all located via the 8-byte Common
 * Header at offset 0:
 *   - Internal Use Area  (optional)
 *   - Chassis Info Area  (optional)
 *   - Board Info Area    (optional)
 *   - Product Info Area  (optional)
 *   - MultiRecord Area   (optional, one or more typed records)
 *
 *
 * The class inherits ParserInterface and overrides parse(),
 * readKeywordFromHardware().
 */
class IpmiFruParser final : public ParserInterface
{
  public:
    IpmiFruParser() = delete;
    IpmiFruParser(const IpmiFruParser&) = delete;
    IpmiFruParser& operator=(const IpmiFruParser&) = delete;
    IpmiFruParser(IpmiFruParser&&) = delete;
    IpmiFruParser& operator=(IpmiFruParser&&) = delete;
    ~IpmiFruParser() = default;

    /**
     * @brief Constructor.
     *
     * @param[in] i_vpdVector  - Raw bytes of the FRU EEPROM image.
     */
    explicit IpmiFruParser(types::BinaryVector i_vpdVector) :
        m_vpdVector(std::move(i_vpdVector)),
        m_logger(Logger::getLoggerInstance()), m_fruMap{std::nullopt}
    {
        // do basic validation of the FRU buffer size and common header size
        if (m_vpdVector.size() < COMMON_HEADER_SIZE)
        {
            throw DataException("FRU VPD buffer too small");
        }

        // check the format version in the Common Header (byte 0 bits[3:0])
        if ((m_vpdVector[0] & 0x0F) != IPMI_FRU_FORMAT_VERSION)
        {
            throw DataException("FRU VPD format version mismatch");
        }
    }

    /**
     * @brief Parse the IPMI FRU binary image.
     *
     * Validates the Common Header checksum, then parses each present area.
     * Returns a IPMIVpdMap wrapped in a VPDMapVariant.
     *
     * @throw DataException on structural or checksum errors.
     *
     * @return types::VPDMapVariant.
     */
    types::VPDMapVariant parse() override;

  private:
    /* ------------------------------------------------------------------ */
    /* Internal type definitions                 */
    /* ------------------------------------------------------------------ */

    /* Array which holds area byte offsets.*/
    using AreaByteOffsets =
        std::array<size_t,
                   static_cast<std::size_t>(types::IpmiVpdAreaIndex::SIZE)>;

    /* ------------------------------------------------------------------ */
    /* Internal constants derived from the IPMI FRU spec                  */
    /* ------------------------------------------------------------------ */

    /** Format version expected in every area/header byte 0 bits[3:0]. */
    static constexpr uint8_t IPMI_FRU_FORMAT_VERSION = 0x01;

    /** Common Header size in bytes (always 8). */
    static constexpr size_t COMMON_HEADER_SIZE = 8;

    /** Multiplier to convert offset units to actual byte offsets. */
    static constexpr size_t AREA_OFFSET_MULTIPLIER = 8U;

    /** Keyword name for the language code byte. */
    static constexpr auto KW_PRODUCT_LANGUAGE_CODE = "PLCODE";

    /* ------------------------------------------------------------------ */
    /* Internal helpers                                                    */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Process the Common Header
     *
     * This method processes the Common Header, calculates the byte offset of
     * each area and returns them as an array. This also computes and verifies
     * the Common Header Checksum.
     *
     * @return On success, returns array containing byte offsets of each area,
     * otherwise sets error code.
     */
    std::expected<AreaByteOffsets, error_code> processCommonHeader() noexcept;

    /**
     * @brief Compute the IPMI zero-checksum over a contiguous byte range.
     *
     * Returns the 2's complement modulo-256 sum of all bytes.  A valid checksum
     * region (data bytes + stored checksum byte) must sum to 0.
     *
     * @param[in] i_begin - Start of the byte range.
     * @param[in] i_end   - One-past-end of the byte range.
     * @return uint8_t - Modulo-256 sum.
     */
    std::expected<uint8_t, error_code> computeChecksum(
        types::BinaryVector::const_iterator i_begin,
        types::BinaryVector::const_iterator i_end) const noexcept;

    /**
     * @brief Decode the seven predefined variable-length fields of the
     *        Product Info Area into a keyword/value map.
     *
     * Walks the ordered list of predefined fields (MNAME, PNAME, PPMN, PVER,
     * PSN, ASSET_TAG, FRU_FILE_ID) from @p i_pos up to @p i_end, calling
     * decodeField() for each one and inserting the result into @p o_map.
     * Stops early (without error) if the end-of-fields sentinel (0xC1) is
     * encountered before all seven fields have been read.
     *
     * @param[in,out] i_pos - Iterator at the first type/length byte of the
     *                        predefined field sequence; advanced past every
     *                        consumed byte on success.
     * @param[in]     i_end - One-past-end of the safe field region (i.e.
     *                        excludes the area checksum byte).
     * @param[out]    o_map - Map to receive the decoded keyword/value pairs.
     *
     * @return constants::VALUE_0 on success, or an error_code on the first
     *         field that fails to decode or when data is exhausted before the
     *         sentinel.
     */
    std::expected<uint8_t, error_code> parsePredefinedProductFields(
        types::BinaryVector::const_iterator& i_pos,
        types::BinaryVector::const_iterator i_end,
        types::IPMIVpdValueMap& o_map) noexcept;

    /**
     * @brief Parse the Product Info Area
     *
     * @param[in] i_offset - Byte offset of the area.
     *
     * @return On success, returns 0, otherwise returns error code
     */
    std::expected<uint8_t, error_code> parseProductInfoArea(
        const size_t i_offset) noexcept;

    /* ------------------------------------------------------------------ */
    /* Member data                                                         */
    /* ------------------------------------------------------------------ */

    /** Raw EEPROM bytes
     */
    types::BinaryVector m_vpdVector;

    /** Shared logger instance. */
    std::shared_ptr<Logger> m_logger;

    /**
     * Map for holding parsed IPMI FRU data. The format is:
     *   <AreaName, <Keyword, Value>>
     * For example, for Manufacturer Name in the Product Info Area, the map
     * entry will be of format:
     *   <"Product Info Area", <"MNAME", "IBM">>
     */
    types::IPMIVpdMap m_fruMap;
};

} // namespace vpd
