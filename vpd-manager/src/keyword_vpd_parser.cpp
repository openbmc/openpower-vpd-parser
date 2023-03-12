#include "keyword_vpd_parser.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"

#include <iostream>
#include <numeric>
#include <string>

namespace vpd
{

types::VPDMapVariant KeywordVpdParser::parse()
{
    if (m_keywordVpdVector.empty())
    {
        throw(DataException("Vector for Keyword format VPD is empty"));
    }
    m_vpdIterator = m_keywordVpdVector.begin();

    if (*m_vpdIterator != constants::KW_VPD_START_TAG)
    {
        throw(DataException("Invalid Large resource type Identifier String"));
    }

    checkNextBytesValidity(sizeof(constants::KW_VPD_START_TAG));
    std::advance(m_vpdIterator, sizeof(constants::KW_VPD_START_TAG));

    uint16_t l_dataSize = getKwDataSize();

    checkNextBytesValidity(constants::TWO_BYTES + l_dataSize);
    std::advance(m_vpdIterator, constants::TWO_BYTES + l_dataSize);

    // Check for invalid vendor defined large resource type
    if (*m_vpdIterator != constants::KW_VPD_PAIR_START_TAG)
    {
        if (*m_vpdIterator != constants::ALT_KW_VPD_PAIR_START_TAG)
        {
            throw(DataException("Invalid Keyword Vpd Start Tag"));
        }
    }
    types::BinaryVector::const_iterator l_checkSumStart = m_vpdIterator;
    auto l_kwValMap = populateVpdMap();

    // Do these validations before returning parsed data.
    // Check for small resource type end tag
    if (*m_vpdIterator != constants::KW_VAL_PAIR_END_TAG)
    {
        throw(DataException("Invalid Small resource type End"));
    }

    types::BinaryVector::const_iterator l_checkSumEnd = m_vpdIterator;
    validateChecksum(l_checkSumStart, l_checkSumEnd);

    checkNextBytesValidity(constants::TWO_BYTES);
    std::advance(m_vpdIterator, constants::TWO_BYTES);

    // Check VPD end Tag.
    if (*m_vpdIterator != constants::KW_VPD_END_TAG)
    {
        throw(DataException("Invalid Small resource type."));
    }

    return l_kwValMap;
}

types::KeywordVpdMap KeywordVpdParser::populateVpdMap()
{
    checkNextBytesValidity(constants::ONE_BYTE);
    std::advance(m_vpdIterator, constants::ONE_BYTE);

    auto l_totalSize = getKwDataSize();
    if (l_totalSize == 0)
    {
        throw(DataException("Data size is 0, badly formed keyword VPD"));
    }

    checkNextBytesValidity(constants::TWO_BYTES);
    std::advance(m_vpdIterator, constants::TWO_BYTES);

    types::KeywordVpdMap l_kwValMap;

    // Parse the keyword-value and store the pairs in map
    while (l_totalSize > 0)
    {
        checkNextBytesValidity(constants::TWO_BYTES);
        std::string l_keywordName(m_vpdIterator,
                                  m_vpdIterator + constants::TWO_BYTES);
        std::advance(m_vpdIterator, constants::TWO_BYTES);

        size_t l_kwSize = *m_vpdIterator;
        checkNextBytesValidity(constants::ONE_BYTE + l_kwSize);
        m_vpdIterator++;
        std::vector<uint8_t> l_valueBytes(m_vpdIterator,
                                          m_vpdIterator + l_kwSize);
        std::advance(m_vpdIterator, l_kwSize);

        l_kwValMap.emplace(
            std::make_pair(std::move(l_keywordName), std::move(l_valueBytes)));

        l_totalSize -= constants::TWO_BYTES + constants::ONE_BYTE + l_kwSize;
    }

    return l_kwValMap;
}

void KeywordVpdParser::validateChecksum(
    types::BinaryVector::const_iterator i_checkSumStart,
    types::BinaryVector::const_iterator i_checkSumEnd)
{
    uint8_t l_checkSumCalculated = 0;

    // Checksum calculation
    l_checkSumCalculated = std::accumulate(i_checkSumStart, i_checkSumEnd,
                                           l_checkSumCalculated);
    l_checkSumCalculated = ~l_checkSumCalculated + 1;
    uint8_t l_checksumVpdValue = *(m_vpdIterator + constants::ONE_BYTE);

    if (l_checkSumCalculated != l_checksumVpdValue)
    {
        throw(DataException("Invalid Checksum"));
    }
}

void KeywordVpdParser::checkNextBytesValidity(uint8_t i_numberOfBytes)
{
    if ((std::distance(m_keywordVpdVector.begin(),
                       m_vpdIterator + i_numberOfBytes)) >
        std::distance(m_keywordVpdVector.begin(), m_keywordVpdVector.end()))
    {
        throw(DataException("Truncated VPD data"));
    }
}

} // namespace vpd
