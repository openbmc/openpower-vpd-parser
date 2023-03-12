#pragma once

#include "parser_interface.hpp"
#include "types.hpp"

namespace vpd
{

/**
 * @brief Concrete class to implement Keyword VPD parsing
 *
 * The class inherits ParserInterface class and overrides the parser
 * functionality to implement parsing logic for Keyword VPD format.
 */
class KeywordVpdParser : public ParserInterface
{
  public:
    KeywordVpdParser() = delete;
    KeywordVpdParser(const KeywordVpdParser&) = delete;
    KeywordVpdParser(KeywordVpdParser&&) = delete;
    ~KeywordVpdParser() = default;

    /**
     * @brief Constructor
     *
     * @param kwVpdVector - move it to object's m_keywordVpdVector
     */
    KeywordVpdParser(const types::BinaryVector& kwVpdVector) :
        m_keywordVpdVector(kwVpdVector),
        m_vpdIterator(m_keywordVpdVector.begin())
    {}

    /**
     * @brief A wrapper function to parse the keyword VPD binary data.
     *
     * It validates certain tags and checksum data, calls helper function
     * to parse and emplace the data as keyword-value pairs in KeywordVpdMap.
     *
     * @throw DataException - VPD is not valid
     * @return map of keyword:value
     */
    types::VPDMapVariant parse();

  private:
    /**
     * @brief Parse the VPD data and emplace them as pair into the Map.
     *
     * @throw DataException - VPD data size is 0, check VPD
     * @return map of keyword:value
     */
    types::KeywordVpdMap populateVpdMap();

    /**
     * @brief Validate checksum.
     *
     * Finding the 2's complement of sum of all the
     * keywords,values and large resource identifier string.
     *
     * @param[in] i_checkSumStart - VPD iterator pointing at checksum start
     * value
     * @param[in] i_checkSumEnd - VPD iterator pointing at checksum end value
     * @throw DataException - checksum invalid, check VPD
     */
    void validateChecksum(types::BinaryVector::const_iterator i_checkSumStart,
                          types::BinaryVector::const_iterator i_checkSumEnd);

    /**
     * @brief It reads 2 bytes from current VPD pointer
     *
     * @return 2 bytes of VPD data
     */
    inline size_t getKwDataSize()
    {
        return (*(m_vpdIterator + 1) << 8 | *m_vpdIterator);
    }

    /**
     * @brief Check for given number of bytes validity
     *
     * Check if number of elements from (begining of the vector) to (iterator +
     * numberOfBytes) is lesser than or equal to the total no.of elements in
     * the vector. This check is performed before advancement of the iterator.
     *
     * @param[in] numberOfBytes - no.of positions the iterator is going to be
     * iterated
     *
     * @throw DataException - Truncated VPD data, check VPD.
     */
    void checkNextBytesValidity(uint8_t numberOfBytes);

    /*Vector of keyword VPD data*/
    const types::BinaryVector& m_keywordVpdVector;

    /*Iterator to VPD data*/
    types::BinaryVector::const_iterator m_vpdIterator;
};
} // namespace vpd
