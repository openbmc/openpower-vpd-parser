#include "keyword_vpd_parser.hpp"

#include <iostream>
#include <numeric>
#include <string>

namespace vpd
{
namespace keyword
{
namespace parser
{
KeywordVpdMap KeywordVpdParser::parseKwVpd()
{
    int kwVpdType;
    if (keywordVpdVector.empty())
    {
        throw std::runtime_error("Blank Vpd Data");
    }

    validateLargeResourceIdentifierString();

    kwVpdType = validateTheTypeOfKwVpd();

    auto kwValMap = kwValParser();

    // Donot process these two functions for bono type VPD
    if (!kwVpdType)
    {
        validateSmallResourceTypeEnd();

        validateChecksum();
    }

    validateSmallResourceTypeLastEnd();

#ifdef DEBUG_KW_VPD
    cerr << '\n' << " KW " << '\t' << "  VALUE " << '\n';
    for (const auto& it : kwValMap)
    {
        cerr << '\n' << " " << it->first << '\t';
        copy((it->second).begin(), (it->second).end(),
             ostream_iterator<int>(cout << hex, " "));
    }
#endif

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

    itrOutOfBoundCheck(1);
    advance(kwVpdIterator, sizeof(KW_VPD_START_TAG));
}

int KeywordVpdParser::validateTheTypeOfKwVpd()
{
    size_t dataSize = getKwDataSize();

    itrOutOfBoundCheck(TWO_BYTES + dataSize);

#ifdef DEBUG_KW_VPD
    auto dsDeb = dataSize;
    auto itDeb = kwVpdIterator + TWO_BYTES;
    std::cout << '\n' << '\t';
    while (dsDeb != 0)
    {
        std::cout << *itDeb;
        itDeb++;
        dsDeb--;
    }
    std::cout << '\n';
#endif

    // +TWO_BYTES is the description's size byte
    std::advance(kwVpdIterator, TWO_BYTES + dataSize);

    int kwVpdType = 0;
    // Check for invalid vendor defined large resource type
    if (*kwVpdIterator != KW_VAL_PAIR_START_TAG)
    {
        if (*kwVpdIterator != ALT_KW_VAL_PAIR_START_TAG)
        {
            throw std::runtime_error("Invalid Keyword Value Pair Start Tag");
        }
        // Bono vpd referred as 1
        kwVpdType = 1;
    }
    return kwVpdType;
}

KeywordVpdMap KeywordVpdParser::kwValParser()
{
    int totalSize = 0;
    KeywordVpdMap kwValMap;

    checkSumStart = kwVpdIterator;

    itrOutOfBoundCheck(1);
    kwVpdIterator++;

    // Get the total length of all keyword value pairs
    totalSize = getKwDataSize();

    if (totalSize == 0)
    {
        throw std::runtime_error("Badly formed keyword VPD data");
    }

    itrOutOfBoundCheck(TWO_BYTES);
    std::advance(kwVpdIterator, TWO_BYTES);

    // Parse the keyword-value and store the pairs in map
    while (totalSize > 0)
    {
        std::string kwStr(kwVpdIterator, kwVpdIterator + TWO_BYTES);

        totalSize -= TWO_BYTES;
        itrOutOfBoundCheck(TWO_BYTES);
        std::advance(kwVpdIterator, TWO_BYTES);

        size_t kwSize = *kwVpdIterator;

        itrOutOfBoundCheck(1);
        kwVpdIterator++;

        std::vector<uint8_t> valVec(kwVpdIterator, kwVpdIterator + kwSize);

        itrOutOfBoundCheck(kwSize);
        std::advance(kwVpdIterator, kwSize);

        totalSize -= kwSize + 1;

        kwValMap.emplace(std::make_pair(std::move(kwStr), std::move(valVec)));
    }

    checkSumEnd = kwVpdIterator - 1;

    return kwValMap;
}

void KeywordVpdParser::validateSmallResourceTypeEnd()
{
    // Check for small resource type end tag
    if (*kwVpdIterator != KW_VAL_PAIR_END_TAG)
    {
        throw std::runtime_error("Invalid Small resource type End");
    }
}

void KeywordVpdParser::validateChecksum()
{
    uint8_t checkSum = 0;

    // Checksum calculation
    checkSum = std::accumulate(checkSumStart, checkSumEnd + 1, checkSum);
    checkSum = ~checkSum + 1;

    if (checkSum != *(kwVpdIterator + 1))
    {
        throw std::runtime_error("Invalid Check sum");
    }
#ifdef DEBUG_KW_VPD
    std::cout << "\nCHECKSUM : " << std::hex << static_cast<int>(checkSum)
              << std::endl;
#endif

    itrOutOfBoundCheck(TWO_BYTES);
    std::advance(kwVpdIterator, TWO_BYTES);
}

void KeywordVpdParser::validateSmallResourceTypeLastEnd()
{
    // Check for small resource type last end of data
    if (*kwVpdIterator != KW_VPD_END_TAG)
    {
        throw std::runtime_error(
            "Invalid Small resource type Last End Of Data");
    }
}

size_t KeywordVpdParser::getKwDataSize()
{
    return (*(kwVpdIterator + 1) << 8 | *kwVpdIterator);
}

void KeywordVpdParser::itrOutOfBoundCheck(uint8_t incVar)
{

    if ((std::distance(keywordVpdVector.begin(), kwVpdIterator + incVar)) >
        std::distance(keywordVpdVector.begin(), keywordVpdVector.end()))
    {
        throw std::runtime_error("Badly formed VPD data");
    }
}
} // namespace parser
} // namespace keyword
} // namespace vpd
