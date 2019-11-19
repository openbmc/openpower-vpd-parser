#pragma once

#include "keyword_vpd_types.hpp"

namespace vpd
{
namespace keyword
{
namespace parser
{

/**
 * @class KeywordVpdParser
 * @brief Implements parser for Keyword VPD
 *
 * KeywordVpdParser object must be constructed by passing in
 * Keyword VPD in binary format. To parse the VPD, call the
 * kwVpdParser() method. The kwVpdParser() method returns
 * a map of keyword-value pairs.
 *
 * Following is the algorithm used to parse Keyword VPD data:
 * 1) Validate if the first byte is 'largeResourceIdentifierString'.
 * 2) Validate the byte after the description is 'vendor defined large resource
 * type tag'.
 * 3) For each keyword-value pairs :
 * 	3.1) Parse the 2 byte length keyword and emplace it in the map as 'key'.
 * 	3.2) Parse over the value bytes corresponding to the keyword and
 * 	     emplace it in the map as 'value' for the key inserted in 3.1.
 * 4) Validate the byte before checksum byte is 'small resource type end tag'.
 * 5) Validate the checksum.
 * 6) Validate the 'small resource type last end tag'.
 * 7) Return the keyword-value map.
 */

class KeywordVpdParser
{
  public:
    KeywordVpdParser() = delete;
    KeywordVpdParser(const KeywordVpdParser&) = delete;
    KeywordVpdParser(KeywordVpdParser&&) = delete;
    ~KeywordVpdParser() = default;

    /**
     * @brief Move Constructor
     *
     * Move kwVpdVector to parser object's kwVpdVector
     */
    KeywordVpdParser(Binary&& kwVpdVector) :
        keywordVpdVector(std::move(kwVpdVector))
    {
    }

    /**
     * @brief Parse the keyword VPD binary data.
     *
     * Calls the sub functions to emplace the
     * keyword-value pairs in map and to validate
     * certain tags and checksum data.
     *
     * @return map of keyword:value
     */
    KeywordVpdMap parseKwVpd();

  private:
    Binary::iterator checkSumStart; //!< Pointer to the start byte from where
                                    //!< the checksum need to be calculated
    Binary::iterator checkSumEnd;   //!< Pointer to the end byte until which the
                                    //!< checksum need to be calculated
    Binary::iterator kwVpdIterator; //!< Iterator to parse the vector
    Binary keywordVpdVector;        //!< Vector which stores keyword VPD data

    /**
     * @brief Validate the large resource identifier string
     */
    void validateLargeResourceIdentifierString();

    /**
     * @brief Validate the type of keyword VPD
     *
     * @return integer representing the type of kw VPD.
     */
    int validateTheTypeOfKwVpd();

    /**
     * @brief Parsing keyword-value pairs and emplace into Map.
     *
     * @return map of keyword:value
     */
    KeywordVpdMap kwValParser();

    /**
     * @brief Validate small resource type end tag
     */
    void validateSmallResourceTypeEnd();

    /**
     * @brief Validate checksum.
     *
     * Finding the 2's complement of sum of all the
     * keywords,values and large resource identifier string.
     */
    void validateChecksum();

    /**
     * @brief Validate small resource type last end tag
     */
    void validateSmallResourceTypeLastEnd();

    /**
     * @brief Get the size of the keyword
     *
     * @return one byte length size data
     */
    size_t getKwDataSize();

    /**
     * @brief Check for iterator Out of Bound exception
     *
     * Check if no.of elements from (begining of the vector) to (iterator +
     * incVar) is lesser than or equal to the total no.of elements in the
     * vector. This check is performed before the advancement of the iterator.
     *
     * @param[incVar] - no.of positions the iterator is going to be iterated
     */
    void itrOutOfBoundCheck(uint8_t incVar);
};
} // namespace parser
} // namespace keyword
} // namespace vpd
