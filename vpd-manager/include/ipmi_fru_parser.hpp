#pragma once

#include "logger.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

#include <fstream>
#include <optional>
#include <string>

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
 *   - Internal Use Area  (optional, opaque)
 *   - Chassis Info Area  (optional)
 *   - Board Info Area    (optional)
 *   - Product Info Area  (optional)
 *   - MultiRecord Area   (optional, one or more typed records)
 *
 * Parsed fields are stored in a KeywordVpdMap using flat string keys of the
 * form "<AreaPrefix>_<FieldName>", e.g. "BOARD_Manufacturer",
 * "PRODUCT_SerialNumber", "CHASSIS_PartNumber".
 *
 * Custom fields (OEM-defined) within each area are emitted as
 * "<AreaPrefix>_Custom<N>" (1-based index).
 *
 * The class inherits ParserInterface and overrides parse(),
 * readKeywordFromHardware(), and writeKeywordOnHardware().
 */
class IpmiFruParser : public ParserInterface
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
     * @param[in] i_vpdFilePath - Path to the EEPROM sysfs file (needed for
     *                            write-back).  May be empty for parse-only.
     */
    explicit IpmiFruParser(types::BinaryVector i_vpdVector,
                           const std::string& i_vpdFilePath = "") :
        m_vpdVector(std::move(i_vpdVector)), m_vpdFilePath(i_vpdFilePath),
        m_logger(Logger::getLoggerInstance())
    {
        if (!m_vpdFilePath.empty())
        {
            try
            {
                m_vpdFileStream.exceptions(
                    std::ifstream::badbit | std::ifstream::failbit);
                m_vpdFileStream.open(m_vpdFilePath,
                                     std::ios::in | std::ios::out |
                                         std::ios::binary);
            }
            catch (const std::fstream::failure& l_ex)
            {
                m_logger->logMessage(
                    std::format("IpmiFruParser: failed to open [{}]: {}",
                                m_vpdFilePath, l_ex.what()));
            }
        }
    }

    /**
     * @brief Parse the IPMI FRU binary image.
     *
     * Validates the Common Header checksum, then parses each present area.
     * Returns a KeywordVpdMap wrapped in a VPDMapVariant.
     *
     * Area/field keys follow the convention described in the class doc.
     *
     * @throw DataException on structural or checksum errors.
     *
     * @return types::VPDMapVariant holding types::KeywordVpdMap.
     */
    types::VPDMapVariant parse() override;

    /**
     * @brief Read a single keyword value from the already-parsed data.
     *
     * @param[in] i_params - types::Keyword naming the key to read
     *                       (e.g. "PRODUCT_SerialNumber").
     *
     * @throw sdbusplus InvalidArgument if keyword is not found.
     *
     * @return types::BinaryVector containing the field bytes.
     */
    types::DbusVariantType readKeywordFromHardware(
        const types::ReadVpdParams i_params) override;

    /**
     * @brief Write a keyword value back to the EEPROM file.
     *
     * Finds the keyword in the parsed map, locates its byte offset in the
     * raw vector, writes the new value (clamped to the original field size),
     * and recalculates the area checksum.
     *
     * @param[in] i_params - types::KwData{keyword, value}.
     *
     * @throw DataException if keyword not found, value empty, or file not open.
     * @throw sdbusplus InvalidArgument on bad parameter type.
     *
     * @return Number of bytes written.
     */
    int writeKeywordOnHardware(const types::WriteVpdParams i_params) override;

  private:
    /* ------------------------------------------------------------------ */
    /* Internal constants derived from the IPMI FRU spec                  */
    /* ------------------------------------------------------------------ */

    /** Format version expected in every area/header byte 0 bits[3:0]. */
    static constexpr uint8_t IPMI_FRU_FORMAT_VERSION = 0x01;

    /** End-of-fields sentinel byte (type/length = 0xC1). */
    static constexpr uint8_t FIELD_END_SENTINEL = 0xC1;

    /** Common Header size in bytes (always 8). */
    static constexpr size_t COMMON_HEADER_SIZE = 8;

    /** Area/record length fields are in multiples of 8 bytes. */
    static constexpr size_t AREA_UNIT_BYTES = 8;

    /** MultiRecord header is always 5 bytes. */
    static constexpr size_t MULTIRECORD_HDR_SIZE = 5;

    /** Bit 7 of byte 1 of a MultiRecord header = End-of-list flag. */
    static constexpr uint8_t MULTIRECORD_EOL_BIT = 0x80;

    /** Type/length byte: type code is bits [7:6]. */
    static constexpr uint8_t TYPELEN_TYPE_MASK = 0xC0;
    static constexpr uint8_t TYPELEN_TYPE_SHIFT = 6;

    /** Type/length byte: length is bits [5:0]. */
    static constexpr uint8_t TYPELEN_LEN_MASK = 0x3F;

    /** Type codes. */
    static constexpr uint8_t TYPECODE_BINARY = 0x00;
    static constexpr uint8_t TYPECODE_BCD_PLUS = 0x01;
    static constexpr uint8_t TYPECODE_6BIT_ASCII = 0x02;
    static constexpr uint8_t TYPECODE_8BIT_ASCII = 0x03;

    /* ------------------------------------------------------------------ */
    /* Internal helpers                                                    */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Validate a zero-sum checksum over [begin, end) + checkByte == 0.
     *
     * @param[in] i_begin     - First byte to include in the sum.
     * @param[in] i_end       - One-past-last byte to include.
     * @param[in] i_checkByte - The stored checksum byte.
     * @param[in] i_context   - Description used in error messages.
     *
     * @throw DataException if the checksum is invalid.
     */
    void validateChecksum(types::BinaryVector::const_iterator i_begin,
                          types::BinaryVector::const_iterator i_end,
                          uint8_t i_checkByte,
                          const std::string& i_context) const;

    /**
     * @brief Decode a type/length-prefixed field string.
     *
     * Handles Binary, BCD+, 6-bit packed ASCII, and 8-bit ASCII types per
     * spec section 13.
     *
     * @param[in] i_typelen  - The type/length byte.
     * @param[in] i_data     - Iterator pointing at the first data byte.
     * @param[in] i_langCode - Language code of the enclosing area (0 =
     * English).
     *
     * @return Decoded string.  Binary fields are returned as hex pairs
     *         separated by spaces.
     */
    std::string decodeField(uint8_t i_typelen,
                            types::BinaryVector::const_iterator i_data,
                            uint8_t i_langCode) const;

    /**
     * @brief Decode a 6-bit packed ASCII field.
     *
     * Four 6-bit characters are packed into three bytes.  Each 6-bit value
     * maps to ASCII 0x20 + value (space through underscore).
     *
     * @param[in] i_data - Iterator at first byte of packed data.
     * @param[in] i_len  - Number of DATA bytes (NOT number of characters).
     *
     * @return Decoded ASCII string with trailing spaces removed.
     */
    std::string decode6BitAscii(types::BinaryVector::const_iterator i_data,
                                size_t i_len) const;

    /**
     * @brief Validate the Internal Use Area format version byte.
     *
     * The area extent is determined by the caller (parse()), which stores
     * raw bytes [offset+1 .. next_area_start) in m_fruMap["INTERNAL_Data"].
     *
     * @param[in] i_offset - Byte offset of the area in m_vpdVector.
     *
     * @throw DataException if the area is truncated.
     */
    void parseInternalUseArea(size_t i_offset);

    /**
     * @brief Parse the Chassis Info Area.
     *
     * Populates keys: CHASSIS_Type, CHASSIS_PartNumber,
     * CHASSIS_SerialNumber, CHASSIS_Custom<N>.
     *
     * @param[in] i_offset - Byte offset of the area.
     */
    void parseChassisInfoArea(size_t i_offset);

    /**
     * @brief Parse the Board Info Area.
     *
     * Populates keys: BOARD_LanguageCode, BOARD_MfgDateTime,
     * BOARD_Manufacturer, BOARD_ProductName, BOARD_SerialNumber,
     * BOARD_PartNumber, BOARD_FruFileId, BOARD_Custom<N>.
     *
     * @param[in] i_offset - Byte offset of the area.
     */
    void parseBoardInfoArea(size_t i_offset);

    /**
     * @brief Parse the Product Info Area.
     *
     * Populates keys: PRODUCT_LanguageCode, PRODUCT_Manufacturer,
     * PRODUCT_ProductName, PRODUCT_PartModelNumber, PRODUCT_Version,
     * PRODUCT_SerialNumber, PRODUCT_AssetTag, PRODUCT_FruFileId,
     * PRODUCT_Custom<N>.
     *
     * @param[in] i_offset - Byte offset of the area.
     */
    void parseProductInfoArea(size_t i_offset);

    /**
     * @brief Parse the MultiRecord Area.
     *
     * Each record is stored under key "MULTIRECORD_<N>" (1-based) as a
     * BinaryVector of the raw record data bytes (not including the 5-byte
     * header).  The record type ID is stored as "MULTIRECORD_<N>_TypeId".
     *
     * @param[in] i_offset - Byte offset of the first record header.
     */
    void parseMultiRecordArea(size_t i_offset);

    /**
     * @brief Read a type/length-prefixed field from the vector, advance
     *        the iterator past the field, and return the decoded string plus
     *        the raw byte offset of the data in m_vpdVector.
     *
     * Returns std::nullopt if the type/length byte is the end sentinel (0xC1).
     * Throws DataException if the field would read past the area end.
     *
     * @param[in,out] io_it     - Iterator pointing at the type/length byte.
     * @param[in]     i_areaEnd - One-past the last valid byte of the area.
     * @param[in]     i_lang    - Language code for encoding decision.
     * @param[out]    o_offset  - Byte offset of the data start in m_vpdVector.
     * @param[out]    o_rawLen  - Raw data length in bytes (field capacity).
     *
     * @return Decoded string, or std::nullopt for the end sentinel.
     */
    std::optional<std::string> readNextField(
        types::BinaryVector::const_iterator& io_it,
        types::BinaryVector::const_iterator i_areaEnd, uint8_t i_lang,
        size_t& o_offset, size_t& o_rawLen) const;

    /**
     * @brief Recompute and write back the area checksum at the given offset.
     *
     * The checksum is the last byte of the area (offset = areaStart +
     * areaLength*8 - 1).  It is the 2's complement of the sum of all
     * preceding bytes in the area (modulo 256).
     *
     * @param[in] i_areaStart  - Byte offset of area start in m_vpdVector.
     * @param[in] i_areaLength - Area length in 8-byte units (from header byte
     * 1).
     *
     * Also writes the new checksum byte back to m_vpdVector so subsequent
     * checksum recalculations see the updated data.
     */
    void rewriteAreaChecksum(size_t i_areaStart, size_t i_areaLength);

    /* ------------------------------------------------------------------ */
    /* Member data                                                         */
    /* ------------------------------------------------------------------ */

    /** Raw EEPROM bytes (mutable so write-back can update the in-memory image).
     */
    types::BinaryVector m_vpdVector;

    /** Path to the EEPROM sysfs file (may be empty). */
    const std::string m_vpdFilePath;

    /** Open file stream for write-back (valid only when m_vpdFilePath != "").
     */
    std::fstream m_vpdFileStream;

    /**
     * Accumulated parse results keyed as "<AREA>_<Field>".
     * Populated during parse(); used by readKeywordFromHardware().
     */
    types::KeywordVpdMap m_fruMap;

    /**
     * Maps keyword name → {byte-offset-in-m_vpdVector, field-capacity-bytes}.
     * Needed by writeKeywordOnHardware() to locate the field in the raw image.
     */
    std::unordered_map<std::string, std::pair<size_t, size_t>> m_fieldOffsets;

    /**
     * Maps keyword name → {area-start-offset, area-length-units}.
     * Needed to recompute the area checksum after a write.
     */
    std::unordered_map<std::string, std::pair<size_t, size_t>> m_fieldAreaInfo;

    /** Shared logger instance. */
    std::shared_ptr<Logger> m_logger;
};

} // namespace vpd
