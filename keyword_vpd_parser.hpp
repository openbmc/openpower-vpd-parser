#pragma once

#include "keyword_vpd_main.hpp"

#include <unordered_map>

namespace vpd
{
namespace keyword
{
namespace parser
{

/** @class KeywordVpdParser
 *  @brief Implements parser for Keyword VPD
 *
 *  KeywordVpdParser object must be constructed by passing in
 *  Keyword VPD in binary format. To parse the VPD, call the
 *  kwVpdParser() method. The kwVpdParser() method returns
 *  a map of keyword-value pairs.
 *
 *  Following is the algorithm used to parse Keyword VPD data:
 *  1) Validate if the first byte is 'largeResourceIdentifierString'.
 *  2) Validate the byte after the description is 'large resource type vendor
 * defined tag'. 3) For each keyword-value pairs : 3.1) Parse the 2 byte length
 * keyword and emplace it in the map as 'key'. 3.2) Parse over the value bytes
 * corresponding to the keyword and emplace it in the map as 'value' for the key
 * inserted in 3.1. 4) Return the map 5) Validate the checksum
 */

class KeywordVpdParser
{
  private:
    binary::iterator checkSumStart;
    binary::iterator checkSumEnd;
    binary::iterator kwVpdIterator;
    binary keywordVpdVector;

    /* Keyword VPD Parser */

    /** @brief Validate the large resource identifier string */
    void validateLargeResourceIdentifierString();

    /** @brief Parsing keyword-value pairs and emplace into Map.
     *
     *  @returns map of keyword:value
     */
    map kwValParser();

    /** @brief Validate checksum.
     *
     *  Finding the 2's complement of sum of all the
     *  keywords,values and large resource identifier string.
     *
     *  Following is the algorithm to validate checksum:
     *  1) Add all the bytes covered.
     *  2) Drop any carry out of the running 1 byte sum.
     *  3) Invert all the bits
     *  4) Add 1 to the 1 byte result and throw away any carry
     */
    void validateChecksum();

    /** @brief Get the size of the keyword */
    size_t getKwDataSize();

  public:
    KeywordVpdParser(binary&& kwVpdVector) :
        keywordVpdVector(std::move(kwVpdVector))
    {
    }

    /** @brief Parse the keyword VPD binary data.
     *
     *  Calls the sub functions to emplace the
     *  keyword-value pairs in map and to validate
     *  certain tags and checksum data.
     *
     *  @returns map of keyword:value
     */
    map kwVpdParser();
};
} // namespace parser
} // namespace keyword
} // namespace vpd
