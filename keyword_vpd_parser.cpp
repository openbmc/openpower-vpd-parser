#include "keyword_vpd_parser.hpp"

#include <bits/stdc++.h>

#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace openpower
{
namespace keywordVpd
{
namespace parser
{
std::unordered_map<std::string, std::vector<uint8_t>>
    KeywordVpdParser::kwVpdParser()
{
    largeResourceIdentifierStringParser();

    std::unordered_map<std::string, std::vector<uint8_t>> kwValMap =
        kwValParser();

    checkSumValidation();

    return kwValMap;
}

void KeywordVpdParser::largeResourceIdentifierStringParser()
{
    iterator = keywordVpdVector.begin();

    // Check for large resource type identfier string
    if (*iterator != KW_VPD_START_TAG)
    {
        throw std::runtime_error(
            "Invalid Large resource type Identifier String");
    }

    std::advance(iterator, 1);
}

std::unordered_map<std::string, std::vector<uint8_t>>
    KeywordVpdParser::kwValParser()
{
    size_t dataSize, totalSize;
    std::unordered_map<std::string, std::vector<uint8_t>> kwValMap;

    dataSize = getDataSize(iterator);

    // +2 is the description's size byte
    std::advance(iterator, 2 + dataSize);

    // Check for large resource type vendor defined tag
    if (*iterator != KW_VAL_PAIR_START_TAG)
    {
        throw std::runtime_error("Invalid Large resource type Vendor Defined");
    }

    checkSumStart = iterator;

    iterator++;

    // Get the total length of all keyword value pairs
    totalSize = getDataSize(iterator);

    std::advance(iterator, 2);

    // Parse the keyword-value and store the pairs in map
    while (totalSize > 0)
    {
        std::string kwStr(iterator, iterator + 2);

        totalSize -= 2;
        std::advance(iterator, 2);

        int kwSize = *iterator;

        iterator++;

        std::vector<uint8_t> valVec;
        valVec.insert(valVec.begin(), iterator, iterator + kwSize);

        std::advance(iterator, kwSize);

        totalSize -= kwSize + 1;

        kwValMap.emplace(std::make_pair(kwStr, valVec));
    }

    checkSumEnd = iterator - 1;

    // Check for small resource type end tag
    if (*iterator != KW_VAL_PAIR_END_TAG)
    {
        throw std::runtime_error("Invalid Small resource type End");
    }
    return kwValMap;
}

void KeywordVpdParser::checkSumValidation()
{
    uint8_t checkSum = 0;

    // Checksum calculation
    checkSum = std::accumulate(checkSumStart, checkSumEnd, checkSum);
    checkSum = ~checkSum + 1;

    if (checkSum != *(iterator + 1))
    {
        throw std::runtime_error("\nInvalid Check sum ");
    }

    std::advance(iterator, 2);

    // Check for small resource type last end of data
    if (*iterator != KW_VPD_END_TAG)
    {
        throw std::runtime_error(
            "Invalid Small resource type Last End Of Data ");
    }
}

size_t KeywordVpdParser::getDataSize(Binary::iterator iterator)
{
    return (*(iterator + 1) << 8 | *iterator);
}
} // namespace parser
} // namespace keywordVpd
} // namespace openpower
