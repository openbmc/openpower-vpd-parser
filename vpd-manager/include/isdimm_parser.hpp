#pragma once

#include "logger.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

#include <string_view>

namespace vpd
{
/**
 * @brief Concrete class to implement JEDEC SPD parsing.
 *
 * The class inherits ParserInterface interface class and overrides the parser
 * functionality to implement parsing logic for JEDEC SPD format.
 */
class JedecSpdParser : public ParserInterface
{
  public:
    // Deleted API's
    JedecSpdParser() = delete;
    JedecSpdParser(const JedecSpdParser&) = delete;
    JedecSpdParser& operator=(const JedecSpdParser&) = delete;
    JedecSpdParser(JedecSpdParser&&) = delete;
    JedecSpdParser& operator=(JedecSpdParser&&) = delete;
    ~JedecSpdParser() = default;

    /**
     * @brief Constructor
     *
     * @param[in] i_spdVector - JEDEC SPD data.
     */
    explicit JedecSpdParser(const types::BinaryVector& i_spdVector) :
        m_memSpd(i_spdVector)
    {}

    /**
     * @brief Parse the memory SPD binary data.
     *
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    types::VPDMapVariant parse();

  private:
    /**
     * @brief An API to read keywords.
     *
     * @param[in] i_iterator - iterator to buffer containing SPD
     * @return- map of kwd:value
     */
    types::JedecSpdMap readKeywords(
        types::BinaryVector::const_iterator& i_iterator);

    /**
     * @brief This function calculates DIMM size from DDR4 SPD
     *
     * @param[in] i_iterator - iterator to buffer containing SPD
     * @return calculated size or 0 in case of any error.
     */
    auto getDDR4DimmCapacity(types::BinaryVector::const_iterator& i_iterator);

    /**
     * @brief This function calculates part number from DDR4 SPD
     *
     * @param[in] i_iterator - iterator to buffer containing SPD
     * @return calculated part number or a default value.
     */
    std::string_view getDDR4PartNumber(
        types::BinaryVector::const_iterator& i_iterator);

    /**
     * @brief This function calculates serial number from DDR4 SPD
     *
     * @param[in] i_iterator - iterator to buffer containing SPD
     * @return calculated serial number or a default value.
     */
    std::string getDDR4SerialNumber(
        types::BinaryVector::const_iterator& i_iterator);

    /**
     * @brief This function allocates FRU number based on part number
     *
     * Mappings for FRU number from calculated part number is used
     * for DDR4 ISDIMM.
     *
     * @param[in] i_partNumber - part number of the DIMM
     * @param[in] i_iterator - iterator to buffer containing SPD
     * @return allocated FRU number or a default value
     */
    std::string_view getDDR4FruNumber(
        const std::string& i_partNumber,
        types::BinaryVector::const_iterator& i_iterator);

    /**
     * @brief This function allocates CCIN based on part number for DDR4 SPD
     *
     * @param[in] i_partNumber - part number of the DIMM
     * @return allocated CCIN or a default value.
     */
    std::string_view getDDR4CCIN(const std::string& i_partNumber);

    /**
     * @brief This function returns manufacturer's ID for DDR4 DIMM.
     *
     * @return manufacturer ID.
     */
    types::BinaryVector getDDR4ManufacturerId();

    /**
     * @brief This function calculates DIMM size from DDR5 SPD
     *
     * @param[in] i_iterator - iterator to buffer containing SPD
     * @return calculated size or 0 in case of any error.
     */
    auto getDDR5DimmCapacity(types::BinaryVector::const_iterator& i_iterator);

    /**
     * @brief This function calculates part number from DDR5 SPD
     *
     * @param[in] i_iterator - iterator to buffer containing SPD
     * @return calculated part number or a default value.
     */
    auto getDDR5PartNumber(types::BinaryVector::const_iterator& i_iterator);

    /**
     * @brief This function calculates serial number from DDR5 SPD
     *
     * @param[in] i_iterator - iterator to buffer containing SPD
     * @return calculated serial number.
     */
    auto getDDR5SerialNumber(types::BinaryVector::const_iterator& i_iterator);

    /**
     * @brief This function allocates FRU number based on part number
     *
     * Mappings for FRU number from calculated part number is used
     * for DDR5 ISDIMM.
     *
     * @param[in] i_partNumber - part number of the DIMM
     * @return allocated FRU number.
     */
    auto getDDR5FruNumber(const std::string& i_partNumber);

    /**
     * @brief This function allocates CCIN based on part number for DDR5 SPD
     *
     * @param[in] i_partNumber - part number of the DIMM
     * @return allocated CCIN.
     */
    auto getDDR5CCIN(const std::string& i_partNumber);

    // SPD file to be parsed
    const types::BinaryVector& m_memSpd;

    // Shared pointer to Logger object.
    std::shared_ptr<Logger> m_logger;
};

} // namespace vpd
