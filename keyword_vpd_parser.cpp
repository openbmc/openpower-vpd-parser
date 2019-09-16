#include "keyword_vpd_parser.hpp"

#include <bits/stdc++.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace vpd
{
namespace keyword
{
namespace parser
{
const uint8_t KW_VPD_START_TAG = 0x82;
const uint8_t KW_VPD_END_TAG = 0x78;
const uint8_t KW_VAL_PAIR_START_TAG = 0x84;
const uint8_t KW_VAL_PAIR_END_TAG = 0x79;
const int twoBytes = 2;
const int oneByte = 1;
map KeywordVpdParser::kwVpdParser()
{
    validateLargeResourceIdentifierString();

    auto kwValMap = kwValParser();

    validateChecksum();

    return kwValMap;
}

void KeywordVpdParser::validateLargeResourceIdentifierString()
{
    kwVpdIterator = keywordVpdVector.begin();

    // Check for large resource type identfier string
    if (*kwVpdIterator != KW_VPD_START_TAG)
    {
        throw std::runtime_error(
            "Invalid Large resource type Identifier String");
    }

    std::advance(kwVpdIterator, oneByte);
}

map KeywordVpdParser::kwValParser()
{
    size_t kwDataSize = 0;
    auto totalSize = 0;
    map kwValMap;

    kwDataSize = getKwDataSize();

    // +twoBytes is the description's size byte
    std::advance(kwVpdIterator, twoBytes + kwDataSize);

    // Check for large resource type vendor defined tag
    if (*kwVpdIterator != KW_VAL_PAIR_START_TAG)
    {
        throw std::runtime_error("Invalid Large resource type Vendor Defined");
    }

    checkSumStart = kwVpdIterator;

    kwVpdIterator++;

    // Get the total length of all keyword value pairs
    totalSize = getKwDataSize();

    if (totalSize < 0)
    {
        throw std::runtime_error("Badly formed VPD");
    }
    if (totalSize == 0)
    {
        throw std::runtime_error("Invalid keyword VPD data");
    }

    std::advance(kwVpdIterator, twoBytes);

    // Parse the keyword-value and store the pairs in map
    while (totalSize > 0)
    {
        std::string kwStr(kwVpdIterator, kwVpdIterator + twoBytes);

        totalSize -= twoBytes;
        std::advance(kwVpdIterator, twoBytes);

        size_t kwSize = *kwVpdIterator;

        kwVpdIterator++;

        std::vector<uint8_t> valVec(kwVpdIterator, kwVpdIterator + kwSize);

        if (valVec.size() == valVec.max_size())
        {
            throw std::runtime_error("Buffer overflow");
        }

        std::advance(kwVpdIterator, kwSize);

        totalSize -= kwSize + oneByte;

        kwValMap.emplace(std::make_pair(kwStr, valVec));
    }

    checkSumEnd = kwVpdIterator - oneByte;

    // Check for small resource type end tag
    if (*kwVpdIterator != KW_VAL_PAIR_END_TAG)
    {
        throw std::runtime_error("Invalid Small resource type End");
    }
    return kwValMap;
}

void KeywordVpdParser::validateChecksum()
{
    uint8_t checkSum = 0;

    // Checksum calculation
    checkSum = std::accumulate(checkSumStart, checkSumEnd, checkSum);
    checkSum = ~checkSum + oneByte;

    if (checkSum != *(kwVpdIterator + oneByte))
    {
        throw std::runtime_error("Invalid Check sum ");
    }

    std::advance(kwVpdIterator, twoBytes);

    // Check for small resource type last end of data
    if (*kwVpdIterator != KW_VPD_END_TAG)
    {
        throw std::runtime_error(
            "Invalid Small resource type Last End Of Data ");
    }
}

size_t KeywordVpdParser::getKwDataSize()
{
    const int eightPlaces = 8;
    return (*(kwVpdIterator + oneByte) << eightPlaces | *kwVpdIterator);
}
} // namespace parser
} // namespace keyword
} // namespace vpd
