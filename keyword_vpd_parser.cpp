#include "keyword_vpd_parser.hpp"

#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

namespace vpd
{
namespace keyword
{
namespace parser
{
constexpr uint8_t KW_VPD_START_TAG = 0x82;
constexpr uint8_t KW_VPD_END_TAG = 0x78;
constexpr uint8_t KW_VAL_PAIR_START_TAG = 0x84;
constexpr uint8_t KW_VAL_PAIR_END_TAG = 0x79;
constexpr int twoBytes = 2;

KeywordVpdMap KeywordVpdParser::kwVpdParser()
{
    if (keywordVpdVector.empty())
    {
        throw std::runtime_error("Blank Vpd Data");
    }
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

    advance(kwVpdIterator, 1);
    itrOutOfBoundCheck();
}

KeywordVpdMap KeywordVpdParser::kwValParser()
{
    size_t kwDataSize = 0;
    int totalSize = 0;
    KeywordVpdMap kwValMap;

    kwDataSize = getKwDataSize();

    // +twoBytes is the description's size byte
    std::advance(kwVpdIterator, twoBytes + kwDataSize);
    itrOutOfBoundCheck();

    // Check for large resource type vendor defined tag
    if (*kwVpdIterator != KW_VAL_PAIR_START_TAG)
    {
        throw std::runtime_error("Invalid Large resource type Vendor Defined");
    }

    checkSumStart = kwVpdIterator;

    kwVpdIterator++;
    itrOutOfBoundCheck();

    // Get the total length of all keyword value pairs
    totalSize = getKwDataSize();

    if (totalSize == 0)
    {
        throw std::runtime_error("Badly formed keyword VPD data");
    }

    std::advance(kwVpdIterator, twoBytes);
    itrOutOfBoundCheck();

    // Parse the keyword-value and store the pairs in map
    while (totalSize > 0)
    {
        std::string kwStr(kwVpdIterator, kwVpdIterator + twoBytes);

        totalSize -= twoBytes;
        std::advance(kwVpdIterator, twoBytes);
        itrOutOfBoundCheck();

        size_t kwSize = *kwVpdIterator;

        kwVpdIterator++;
        itrOutOfBoundCheck();

        std::vector<uint8_t> valVec(kwVpdIterator, kwVpdIterator + kwSize);

        std::advance(kwVpdIterator, kwSize);
        itrOutOfBoundCheck();

        totalSize -= kwSize + 1;

        kwValMap.emplace(std::make_pair(kwStr, valVec));
    }

    checkSumEnd = kwVpdIterator - 1;

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
    checkSum = std::accumulate(checkSumStart, checkSumEnd + 1, checkSum);
    checkSum = ~checkSum + 1;

    if (checkSum != *(kwVpdIterator + 1))
    {
        throw std::runtime_error("Invalid Check sum ");
    }

    std::advance(kwVpdIterator, twoBytes);
    itrOutOfBoundCheck();

    // Check for small resource type last end of data
    if (*kwVpdIterator != KW_VPD_END_TAG)
    {
        throw std::runtime_error(
            "Invalid Small resource type Last End Of Data ");
    }
}

size_t KeywordVpdParser::getKwDataSize()
{
    return (*(kwVpdIterator + 1) << 8 | *kwVpdIterator);
}
void KeywordVpdParser::itrOutOfBoundCheck()
{
    if (kwVpdIterator == keywordVpdVector.end())
    {
        throw std::runtime_error("Out of Bound Exception ");
    }
}
} // namespace parser
} // namespace keyword
} // namespace vpd
