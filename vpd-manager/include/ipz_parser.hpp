#pragma once

#include "logger.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

#include <fstream>
#include <string_view>

namespace vpd
{
/**
 * @brief Concrete class to implement IPZ VPD parsing.
 *
 * The class inherits ParserInterface interface class and overrides the parser
 * functionality to implement parsing logic for IPZ VPD format.
 */
class IpzVpdParser : public ParserInterface
{
  public:
    // Deleted APIs
    IpzVpdParser() = delete;
    IpzVpdParser(const IpzVpdParser&) = delete;
    IpzVpdParser& operator=(const IpzVpdParser&) = delete;
    IpzVpdParser(IpzVpdParser&&) = delete;
    IpzVpdParser& operator=(IpzVpdParser&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param[in] vpdVector - VPD data.
     * @param[in] vpdFilePath - Path to VPD EEPROM.
     * @param[in] vpdStartOffset - Offset from where VPD starts in the file.
     * Defaulted to 0.
     */
    IpzVpdParser(const types::BinaryVector& vpdVector,
                 const std::string& vpdFilePath, size_t vpdStartOffset = 0) :
        m_vpdVector(vpdVector), m_vpdFilePath(vpdFilePath),
        m_vpdStartOffset(vpdStartOffset), m_logger(Logger::getLoggerInstance())
    {
        try
        {
            m_vpdFileStream.exceptions(
                std::ifstream::badbit | std::ifstream::failbit);
            m_vpdFileStream.open(vpdFilePath, std::ios::in | std::ios::out |
                                                  std::ios::binary);
        }
        catch (const std::fstream::failure& e)
        {
            logging::logMessage(e.what());
        }
    }

    /**
     * @brief Defaul destructor.
     */
    ~IpzVpdParser() = default;

    /**
     * @brief API to parse IPZ VPD file.
     *
     * Note: Caller needs to check validity of the map returned. Throws
     * exception in certain situation, needs to be handled accordingly.
     *
     * @return parsed VPD data.
     */
    virtual types::VPDMapVariant parse() override;

    /**
     * @brief API to check validity of VPD header.
     *
     * Note: The API throws exception in case of any failure or malformed VDP.
     *
     * @param[in] itrToVPD - Iterator to the beginning of VPD file.
     * Don't change the parameter to reference as movement of passsed iterator
     * to an offset is not required after header check.
     */
    void checkHeader(types::BinaryVector::const_iterator itrToVPD);

    /**
     * @brief API to read keyword's value from hardware
     *
     * @param[in] i_paramsToReadData - Data required to perform read
     *
     * @throw
     * sdbusplus::xyz::openbmc_project::Common::Device::Error::ReadFailure.
     *
     * @return On success return the value read. On failure throw exception.
     */
    types::DbusVariantType readKeywordFromHardware(
        const types::ReadVpdParams i_paramsToReadData);

    /**
     * @brief API to write keyword's value on hardware.
     *
     * @param[in] i_paramsToWriteData - Data required to perform write.
     *
     * @throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument.
     * @throw sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed.
     * @throw DataException
     * @throw EccException
     *
     *
     * @return On success returns number of bytes written on hardware, On
     * failure throws exception.
     */
    int writeKeywordOnHardware(const types::WriteVpdParams i_paramsToWriteData);

  private:
    /**
     * @brief Check ECC of VPD header.
     *
     * @return true/false based on check result.
     */
    bool vhdrEccCheck();

    /**
     * @brief Check ECC of VTOC.
     *
     * @return true/false based on check result.
     */
    bool vtocEccCheck();

    /**
     * @brief Check ECC of a record.
     *
     * Note: Throws exception in case of failure. Caller need to handle as
     * required.
     *
     * @param[in] iterator - Iterator to the record.
     * @return success/failre
     */
    bool recordEccCheck(types::BinaryVector::const_iterator iterator);

    /**
     * @brief API to read VTOC record.
     *
     * The API reads VTOC record and returns the length of PT keyword.
     *
     * Note: Throws exception in case of any error. Caller need to handle as
     * required.
     *
     * @param[in] itrToVPD - Iterator to begining of VPD data.
     * @return Length of PT keyword.
     */
    auto readTOC(types::BinaryVector::const_iterator& itrToVPD);

    /**
     * @brief API to read PT record.
     *
     * Note: Throws exception in case ECC check fails.
     *
     * @param[in] itrToPT - Iterator to PT record in VPD vector.
     * @param[in] ptLength - length of the PT record.
     * @return Pair of list of record's offset and a list of invalid
     * records found during parsing
     */
    std::pair<types::RecordOffsetList, types::InvalidRecordList> readPT(
        types::BinaryVector::const_iterator& itrToPT, auto ptLength);

    /**
     * @brief API to read keyword data based on its encoding type.
     *
     * @param[in] kwdName - Name of the keyword.
     * @param[in] kwdDataLength - Length of keyword data.
     * @param[in] itrToKwdData - Iterator to start of keyword data.
     * @return keyword data, empty otherwise.
     */
    std::string readKwData(std::string_view kwdName, std::size_t kwdDataLength,
                           types::BinaryVector::const_iterator itrToKwdData);

    /**
     * @brief API to read keyword and its value under a record.
     *
     * @param[in] iterator - pointer to the start of keywords under the record.
     * @return keyword-value map of keywords under that record.
     */
    types::IPZVpdMap::mapped_type readKeywords(
        types::BinaryVector::const_iterator& itrToKwds);

    /**
     * @brief API to process a record.
     *
     * @param[in] recordOffset - Offset of the record in VPD.
     */
    void processRecord(auto recordOffset);

    /**
     * @brief Get keyword's value from record
     *
     * @param[in] i_record - Record's name
     * @param[in] i_keyword - Keyword's name
     * @param[in] i_recordDataOffset - Record's offset value
     *
     * @throw std::runtime_error
     *
     * @return On success return bytes read, on failure throws error.
     */
    types::BinaryVector getKeywordValueFromRecord(
        const types::Record& i_recordName, const types::Keyword& i_keywordName,
        const types::RecordOffset& i_recordDataOffset);

    /**
     * @brief Get record's details from VTOC's PT keyword value
     *
     * This API parses through VTOC's PT keyword value and returns the given
     * record's offset, record's length, ECC offset and ECC length.
     *
     * @param[in] i_record - Record's name.
     * @param[in] i_vtocOffset - Offset to VTOC record
     *
     * @return On success return record's details, on failure return empty
     * buffer.
     */
    types::RecordData getRecordDetailsFromVTOC(
        const types::Record& l_recordName,
        const types::RecordOffset& i_vtocOffset);

    /**
     * @brief API to update record's ECC
     *
     * This API is required to update the record's ECC based on the record's
     * current data.
     *
     * @param[in] i_recordDataOffset - Record's data offset
     * @param[in] i_recordDataLength - Record's data length
     * @param[in] i_recordECCOffset - Record's ECC offset
     * @param[in] i_recordECCLength - Record's ECC length
     * @param[in,out] io_vpdVector - FRU VPD in vector to update record's ECC.
     *
     * @throw EccException
     */
    void updateRecordECC(const auto& i_recordDataOffset,
                         const auto& i_recordDataLength,
                         const auto& i_recordECCOffset,
                         size_t i_recordECCLength,
                         types::BinaryVector& io_vpdVector);

    /**
     * @brief API to set record's keyword's value on hardware.
     *
     * @param[in] i_recordName - Record name.
     * @param[in] i_keywordName - Keyword name.
     * @param[in] i_keywordData - Keyword data.
     * @param[in] i_recordDataOffset - Offset to record's data.
     * @param[in,out] io_vpdVector - FRU VPD in vector to read and write
     * keyword's value.
     *
     * @throw DataException
     *
     * @return On success returns number of bytes set. On failure returns -1.
     */
    int setKeywordValueInRecord(const types::Record& i_recordName,
                                const types::Keyword& i_keywordName,
                                const types::BinaryVector& i_keywordData,
                                const types::RecordOffset& i_recordDataOffset,
                                types::BinaryVector& io_vpdVector);

    /**
     * @brief API to process list of invalid records found during parsing
     *
     * This API takes a list of invalid records found while parsing a given
     * EEPROM, logs a predictive PEL with details about the invalid records and
     * then dumps the EEPROM data to filesystem.
     *
     * @param[in] i_invalidRecordList - a list of invalid records
     *
     * @return On success returns true, false otherwise.
     */
    bool processInvalidRecords(
        const types::InvalidRecordList& i_invalidRecordList) const noexcept;

    // Holds VPD data.
    const types::BinaryVector& m_vpdVector;

    // stores parsed VPD data.
    types::IPZVpdMap m_parsedVPDMap{};

    // Holds the VPD file path
    const std::string& m_vpdFilePath;

    // Stream to the VPD file. Required to correct ECC
    std::fstream m_vpdFileStream;

    // VPD start offset. Required for ECC correction.
    size_t m_vpdStartOffset = 0;

    // Shared pointer to Logger object.
    std::shared_ptr<Logger> m_logger;
};
} // namespace vpd
