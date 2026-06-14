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

types::DbusVariantType KeywordVpdParser::readKeywordFromHardware(
    const types::ReadVpdParams i_paramsToReadData)
{
    // Extract keyword from i_paramsToReadData
    const types::Keyword* l_keyword =
        std::get_if<types::Keyword>(&i_paramsToReadData);

    if (l_keyword == nullptr || l_keyword->empty())
    {
        throw types::DbusInvalidArgument();
    }

    if (m_keywordVpdVector.empty())
    {
        throw DataException("VPD vector is empty.");
    }

    // Read keyword's value from vector
    m_vpdIterator = m_keywordVpdVector.begin();

    if (*m_vpdIterator != constants::KW_VPD_START_TAG)
    {
        throw DataException("Invalid keyword VPD start tag.");
    }

    checkNextBytesValidity(sizeof(constants::KW_VPD_START_TAG));
    std::advance(m_vpdIterator, sizeof(constants::KW_VPD_START_TAG));

    // Get large resource string size
    uint16_t l_dataSize = getKwDataSize();

    checkNextBytesValidity(constants::TWO_BYTES + l_dataSize);
    std::advance(m_vpdIterator, constants::TWO_BYTES + l_dataSize);

    if (*m_vpdIterator != constants::KW_VPD_PAIR_START_TAG &&
        *m_vpdIterator != constants::ALT_KW_VPD_PAIR_START_TAG)
    {
        throw DataException("Invalid Keyword-value pair start tag.");
    }

    checkNextBytesValidity(constants::ONE_BYTE);
    std::advance(m_vpdIterator, constants::ONE_BYTE);

    // Get total size of keyword-value pairs
    auto l_totalSize = getKwDataSize();

    if (l_totalSize == 0)
    {
        throw DataException("Data size is 0, badly formed keyword VPD");
    }

    checkNextBytesValidity(constants::TWO_BYTES);
    std::advance(m_vpdIterator, constants::TWO_BYTES);

    while (l_totalSize > 0)
    {
        checkNextBytesValidity(constants::TWO_BYTES);
        std::string l_keywordName(m_vpdIterator,
                                  m_vpdIterator + constants::TWO_BYTES);
        std::advance(m_vpdIterator, constants::TWO_BYTES);

        size_t l_kwSize = *m_vpdIterator;
        checkNextBytesValidity(constants::ONE_BYTE + l_kwSize);
        m_vpdIterator++;

        if (l_keywordName == *l_keyword)
        {
            return types::DbusVariantType{
                types::BinaryVector(m_vpdIterator, m_vpdIterator + l_kwSize)};
        }

        std::advance(m_vpdIterator, l_kwSize);
        l_totalSize -= constants::TWO_BYTES + constants::ONE_BYTE + l_kwSize;
    }

    throw DataException(
        std::format("Keyword [{}] is not found in VPD", *l_keyword));
}

int KeywordVpdParser::writeKeywordOnHardware(
    const types::WriteVpdParams i_paramsToWriteData)
{
    types::Keyword l_keywordName;
    types::BinaryVector l_keywordData;

    // Extract keyword and value from i_paramsToWriteData
    if (const types::KwData* l_kwData =
            std::get_if<types::KwData>(&i_paramsToWriteData))
    {
        l_keywordName = std::get<0>(*l_kwData);
        l_keywordData = std::get<1>(*l_kwData);
    }
    else
    {
        throw types::DbusInvalidArgument();
    }

    if (l_keywordName.empty() || l_keywordData.empty())
    {
        throw types::DbusInvalidArgument();
    }

    if (m_keywordVpdVector.empty())
    {
        throw DataException("VPD vector is empty.");
    }

    // Check file availability once before proceeding
    if (m_vpdFilePath.empty() || !m_vpdFileStream.is_open())
    {
        throw DataException("VPD file path not provided or file not open for write operations.");
    }

    // Create a mutable copy of the VPD vector for checksum calculation
    types::BinaryVector l_vpdVector = m_keywordVpdVector;

    // Update the keyword value in the copy and write to file
    size_t l_bytesWritten = setKeywordValue(l_keywordName, l_keywordData, l_vpdVector);

    // Recalculate checksum based on the modified data and write to file
    updateChecksum(l_vpdVector);

    m_logger->logMessage(
        std::format("{} bytes updated successfully on hardware for keyword: {}",
                    l_bytesWritten, l_keywordName));

    return l_bytesWritten;
}

size_t KeywordVpdParser::setKeywordValue(
    const types::Keyword& i_keywordName,
    const types::BinaryVector& i_keywordData,
    types::BinaryVector& io_vpdVector)
{
    auto l_iterator = io_vpdVector.begin();

    // Validate VPD start tag
    if (*l_iterator != constants::KW_VPD_START_TAG)
    {
        throw DataException("Invalid keyword VPD start tag.");
    }

    std::advance(l_iterator, sizeof(constants::KW_VPD_START_TAG));

    // Get large resource string size
    uint16_t l_dataSize = (*(l_iterator + 1) << 8 | *l_iterator);

    std::advance(l_iterator, constants::TWO_BYTES + l_dataSize);

    // Validate keyword-value pair start tag
    if (*l_iterator != constants::KW_VPD_PAIR_START_TAG &&
        *l_iterator != constants::ALT_KW_VPD_PAIR_START_TAG)
    {
        throw DataException("Invalid Keyword-value pair start tag.");
    }

    std::advance(l_iterator, constants::ONE_BYTE);

    // Get total size of keyword-value pairs
    auto l_totalSize = (*(l_iterator + 1) << 8 | *l_iterator);

    if (l_totalSize == 0)
    {
        throw DataException("Data size is 0, badly formed keyword VPD");
    }

    std::advance(l_iterator, constants::TWO_BYTES);

    // Search for the keyword and update its value
    while (l_totalSize > 0)
    {
        // Read keyword name (2 bytes)
        std::string l_currentKeyword(l_iterator,
                                     l_iterator + constants::TWO_BYTES);
        std::advance(l_iterator, constants::TWO_BYTES);

        // Read keyword size (1 byte)
        size_t l_kwSize = *l_iterator;
        std::advance(l_iterator, constants::ONE_BYTE);

        if (l_currentKeyword == i_keywordName)
        {
            // Found the keyword - update its value
            const auto l_lengthToUpdate =
                i_keywordData.size() <= l_kwSize ? i_keywordData.size()
                                                 : l_kwSize;

            // Update the keyword value in the vector (needed for checksum calculation)
            std::copy(i_keywordData.begin(),
                      i_keywordData.begin() + l_lengthToUpdate, l_iterator);

            // Write the keyword value directly to file at the same time
            const auto l_kwdDataOffset =
                std::distance(io_vpdVector.begin(), l_iterator);
            m_vpdFileStream.seekp(l_kwdDataOffset, std::ios::beg);
            m_vpdFileStream.write(
                reinterpret_cast<const char*>(i_keywordData.data()),
                l_lengthToUpdate);
            m_vpdFileStream.flush();

            return l_lengthToUpdate;
        }

        // Move to next keyword
        std::advance(l_iterator, l_kwSize);
        l_totalSize -= constants::TWO_BYTES + constants::ONE_BYTE + l_kwSize;
    }

    // Keyword not found
    throw DataException(
        std::format("Keyword [{}] is not found in VPD", i_keywordName));
}

void KeywordVpdParser::updateChecksum(types::BinaryVector& io_vpdVector)
{
    auto l_iterator = io_vpdVector.begin();

    // Navigate to checksum start position
    std::advance(l_iterator, sizeof(constants::KW_VPD_START_TAG));
    uint16_t l_dataSize = (*(l_iterator + 1) << 8 | *l_iterator);
    std::advance(l_iterator, constants::TWO_BYTES + l_dataSize);

    auto l_checkSumStart = l_iterator;

    std::advance(l_iterator, constants::ONE_BYTE);
    auto l_totalSize = (*(l_iterator + 1) << 8 | *l_iterator);
    std::advance(l_iterator, constants::TWO_BYTES + l_totalSize);

    // Validate end tag
    if (*l_iterator != constants::KW_VAL_PAIR_END_TAG)
    {
        throw DataException("Invalid Small resource type end");
    }

    auto l_checkSumEnd = l_iterator;

    // Calculate checksum
    uint8_t l_checkSumCalculated = 0;
    l_checkSumCalculated =
        std::accumulate(l_checkSumStart, l_checkSumEnd, l_checkSumCalculated);
    l_checkSumCalculated = ~l_checkSumCalculated + 1;

    // Update checksum in the vector (checksum is at offset +1 from end tag)
    *(l_iterator + constants::ONE_BYTE) = l_checkSumCalculated;

    // Write the checksum directly to file at the same time
    const auto l_checksumOffset =
        std::distance(io_vpdVector.begin(), l_iterator + constants::ONE_BYTE);
    m_vpdFileStream.seekp(l_checksumOffset, std::ios::beg);
    m_vpdFileStream.write(reinterpret_cast<const char*>(&l_checkSumCalculated), 1);
    m_vpdFileStream.flush();
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

    std::cout << "Validating checksum " << std::endl;

    // Checksum calculation
    l_checkSumCalculated =
        std::accumulate(i_checkSumStart, i_checkSumEnd, l_checkSumCalculated);
    l_checkSumCalculated = ~l_checkSumCalculated + 1;
    uint8_t l_checksumVpdValue = *(m_vpdIterator + constants::ONE_BYTE);

    if (l_checkSumCalculated != l_checksumVpdValue)
    {
        throw(DataException("Invalid Checksum"));
    }

    std::cout << "Checksum Validated" << std::endl;
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
