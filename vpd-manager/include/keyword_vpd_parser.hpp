#pragma once

#include "logger.hpp"
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
     * @param[in] i_kwVpdVector - VPD data
     * @param[in] i_vpdFilePath - Path to VPD EEPROM
     */
    KeywordVpdParser(const types::BinaryVector& i_kwVpdVector,
                     const std::string& i_vpdFilePath = "") :
        m_keywordVpdVector(i_kwVpdVector), m_vpdFilePath(i_vpdFilePath),
        m_vpdIterator(m_keywordVpdVector.begin()),
        m_logger(Logger::getLoggerInstance())
    {
        if (!m_vpdFilePath.empty())
        {
            try
            {
                m_vpdFileStream.exceptions(
                    std::ifstream::badbit | std::ifstream::failbit);
                m_vpdFileStream.open(m_vpdFilePath,
                                     std::ios::in | std::ios::out |
                                         std::ios::binary);
            }
            catch (const std::fstream::failure& l_ex)
            {
                m_logger->logMessage(std::format(
                    "Failed to open VPD file for write operations: {}",
                    l_ex.what()));
            }
        }
    }

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

    /**
     * @brief Read keyword's value from hardware.
     *
     * @param[in] i_paramsToReadData - Data required to perform read.
     *
     * @throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument
     * if the input parameter type is incompatible or the keyword is not found.
     *
     * @return On success returns the value read. On failure throws exception.
     */
    types::DbusVariantType readKeywordFromHardware(
        const types::ReadVpdParams i_paramsToReadData) override;

    /**
     * @brief API to write keyword's value on hardware.
     *
     * @param[in] i_paramsToWriteData - Data required to perform write.
     *
     * @throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument
     * if the input parameter type is incompatible or the keyword is not found.
     * @throw DataException if keyword is not found or write fails.
     *
     * @return On success returns number of bytes written on hardware, On
     * failure throws exception.
     */
    int writeKeywordOnHardware(
        const types::WriteVpdParams i_paramsToWriteData) override;

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
     * Check if number of elements from (beginning of the vector) to (iterator +
     * numberOfBytes) is lesser than or equal to the total no.of elements in
     * the vector. This check is performed before advancement of the iterator.
     *
     * @param[in] numberOfBytes - no.of positions the iterator is going to be
     * iterated
     *
     * @throw DataException - Truncated VPD data, check VPD.
     */
    void checkNextBytesValidity(uint8_t numberOfBytes);

    /**
     * @brief Find and update keyword value in the VPD vector
     *
     * @param[in] i_keywordName - Keyword name to find and update
     * @param[in] i_keywordData - New data to write
     * @param[in,out] io_vpdVector - VPD vector to modify
     *
     * @throw DataException if keyword is not found
     *
     * @return Number of bytes written
     */
    size_t setKeywordValue(const types::Keyword& i_keywordName,
                           const types::BinaryVector& i_keywordData,
                           types::BinaryVector& io_vpdVector);

    /**
     * @brief Calculate and update checksum in the VPD vector
     *
     * @param[in,out] io_vpdVector - VPD vector to update with new checksum
     *
     * @throw DataException if VPD structure is invalid
     */
    void updateChecksum(types::BinaryVector& io_vpdVector);

    /*Vector of keyword VPD data*/
    const types::BinaryVector& m_keywordVpdVector;

    /*Path to VPD file*/
    const std::string m_vpdFilePath;

    /*Stream to the VPD file for write operations*/
    std::fstream m_vpdFileStream;

    /*Iterator to VPD data*/
    types::BinaryVector::const_iterator m_vpdIterator;

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger;
};
} // namespace vpd
