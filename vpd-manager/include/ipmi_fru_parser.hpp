#pragma once

#include "exceptions.hpp"
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
        m_logger(Logger::getLoggerInstance())
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
    /* Internal constants derived from the IPMI FRU spec                  */
    /* ------------------------------------------------------------------ */

    /** Format version expected in every area/header byte 0 bits[3:0]. */
    static constexpr uint8_t IPMI_FRU_FORMAT_VERSION = 0x01;

    /** Common Header size in bytes (always 8). */
    static constexpr size_t COMMON_HEADER_SIZE = 8;

    /* ------------------------------------------------------------------ */
    /* Internal helpers                                                    */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Validate the Internal Use Area format version byte.
     *
     * @param[in] i_offset - Byte offset of the area in m_vpdVector.
     *
     * @throw DataException if the area is truncated.
     */
    void parseInternalUseArea(size_t i_offset);

    /**
     * @brief Parse the Chassis Info Area.
     *
     *
     * @param[in] i_offset - Byte offset of the area.
     */
    void parseChassisInfoArea(size_t i_offset);

    /**
     * @brief Parse the Board Info Area.
     *
     * @param[in] i_offset - Byte offset of the area.
     */
    void parseBoardInfoArea(size_t i_offset);

    /**
     * @brief Parse the Product Info Area.
     *
     * @param[in] i_offset - Byte offset of the area.
     */
    void parseProductInfoArea(size_t i_offset);

    /**
     * @brief Parse the MultiRecord Area.
     *
     * @param[in] i_offset - Byte offset of the first record header.
     */
    void parseMultiRecordArea(size_t i_offset);

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
