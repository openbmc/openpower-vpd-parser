#include "config.h"

#include "ipz_parser.hpp"

#include "vpdecc/vpdecc.h"

#include "constants.hpp"
#include "exceptions.hpp"
#include "utility/event_logger_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <nlohmann/json.hpp>

#include <typeindex>

namespace vpd
{

// Offset of different entries in VPD data.
enum Offset
{
    VHDR = 17,
    VHDR_TOC_ENTRY = 29,
    VTOC_PTR = 35,
    VTOC_REC_LEN = 37,
    VTOC_ECC_OFF = 39,
    VTOC_ECC_LEN = 41,
    VTOC_DATA = 13,
    VHDR_ECC = 0,
    VHDR_RECORD = 11
};

// Length of some specific entries w.r.t VPD data.
enum Length
{
    RECORD_NAME = 4,
    KW_NAME = 2,
    RECORD_OFFSET = 2,
    RECORD_MIN = 44,
    RECORD_LENGTH = 2,
    RECORD_ECC_OFFSET = 2,
    VHDR_ECC_LENGTH = 11,
    VHDR_RECORD_LENGTH = 44,
    RECORD_TYPE = 2,
    SKIP_A_RECORD_IN_PT = 14,
    JUMP_TO_RECORD_NAME = 6
}; // enum Length

/**
 * @brief API to read 2 bytes LE data.
 *
 * @param[in] iterator - iterator to VPD vector.
 * @return read bytes.
 */
static uint16_t readUInt16LE(types::BinaryVector::const_iterator iterator)
{
    uint16_t lowByte = *iterator;
    uint16_t highByte = *(iterator + 1);
    lowByte |= (highByte << 8);
    return lowByte;
}

bool IpzVpdParser::vhdrEccCheck()
{
    // To avoid 1 bit flip correction from corrupting the main buffer.
    const types::BinaryVector tempVector = m_vpdVector;
    auto vpdPtr = tempVector.cbegin();

    auto l_status = vpdecc_check_data(
        const_cast<uint8_t*>(&vpdPtr[Offset::VHDR_RECORD]),
        Length::VHDR_RECORD_LENGTH,
        const_cast<uint8_t*>(&vpdPtr[Offset::VHDR_ECC]),
        Length::VHDR_ECC_LENGTH);
    if (l_status == VPD_ECC_CORRECTABLE_DATA)
    {
        const types::PelInfoTuple l_pel(
            types::ErrorType::EccCheckFailed,
            types::SeverityType::Informational, 0, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt);

        Logger::getLoggerInstance()->logMessage(
            std::string("One bit correction for VHDR performed"),
            PlaceHolder::PEL, std::make_optional(&l_pel));
    }
    else if (l_status != VPD_ECC_OK)
    {
        return false;
    }

    return true;
}

bool IpzVpdParser::vtocEccCheck()
{
    auto vpdPtr = m_vpdVector.cbegin();

    std::advance(vpdPtr, Offset::VTOC_PTR);

    // The offset to VTOC could be 1 or 2 bytes long
    auto vtocOffset = readUInt16LE(vpdPtr);

    // Get the VTOC Length
    std::advance(vpdPtr, sizeof(types::RecordOffset));
    auto vtocLength = readUInt16LE(vpdPtr);

    // Get the ECC Offset
    std::advance(vpdPtr, sizeof(types::RecordLength));
    auto vtocECCOffset = readUInt16LE(vpdPtr);

    // Get the ECC length
    std::advance(vpdPtr, sizeof(types::ECCOffset));
    auto vtocECCLength = readUInt16LE(vpdPtr);

    // To avoid 1 bit flip correction from corrupting the main buffer.
    const types::BinaryVector tempVector = m_vpdVector;
    // Reset pointer to start of the vpd,
    // so that Offset will point to correct address
    vpdPtr = tempVector.cbegin();

    auto l_status = vpdecc_check_data(
        const_cast<uint8_t*>(&vpdPtr[vtocOffset]), vtocLength,
        const_cast<uint8_t*>(&vpdPtr[vtocECCOffset]), vtocECCLength);
    if (l_status == VPD_ECC_CORRECTABLE_DATA)
    {
        const types::PelInfoTuple l_pel(
            types::ErrorType::EccCheckFailed,
            types::SeverityType::Informational, 0, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt);

        Logger::getLoggerInstance()->logMessage(
            std::string("One bit correction for VTOC performed"),
            PlaceHolder::PEL, std::make_optional(&l_pel));
    }
    else if (l_status != VPD_ECC_OK)
    {
        return false;
    }

    return true;
}

bool IpzVpdParser::recordEccCheck(types::BinaryVector::const_iterator iterator)
{
    auto recordOffset = readUInt16LE(iterator);

    std::advance(iterator, sizeof(types::RecordOffset));
    auto recordLength = readUInt16LE(iterator);

    if (recordOffset == 0 || recordLength == 0)
    {
        throw(DataException("Invalid record offset or length"));
    }

    std::advance(iterator, sizeof(types::RecordLength));
    auto eccOffset = readUInt16LE(iterator);

    std::advance(iterator, sizeof(types::ECCOffset));
    auto eccLength = readUInt16LE(iterator);

    if (eccLength == 0 || eccOffset == 0)
    {
        throw(EccException("Invalid ECC length or offset."));
    }

    // To avoid 1 bit flip correction from corrupting the main buffer.
    const types::BinaryVector tempVector = m_vpdVector;
    auto vpdPtr = tempVector.cbegin();

    auto l_status = vpdecc_check_data(
        const_cast<uint8_t*>(&vpdPtr[recordOffset]), recordLength,
        const_cast<uint8_t*>(&vpdPtr[eccOffset]), eccLength);

    if (l_status == VPD_ECC_CORRECTABLE_DATA)
    {
        const types::PelInfoTuple l_pel(
            types::ErrorType::EccCheckFailed,
            types::SeverityType::Informational, 0, std::nullopt, std::nullopt,
            std::nullopt, std::nullopt);

        Logger::getLoggerInstance()->logMessage(
            std::string("One bit correction for record performed"),
            PlaceHolder::PEL, std::make_optional(&l_pel));
    }
    else if (l_status != VPD_ECC_OK)
    {
        return false;
    }

    return true;
}

void IpzVpdParser::checkHeader(types::BinaryVector::const_iterator itrToVPD)
{
    if (m_vpdVector.empty() || (Length::RECORD_MIN > m_vpdVector.size()))
    {
        throw(DataException("Malformed VPD"));
    }

    std::advance(itrToVPD, Offset::VHDR);
    auto stop = std::next(itrToVPD, Length::RECORD_NAME);

    std::string record(itrToVPD, stop);
    if ("VHDR" != record)
    {
        throw(DataException("VHDR record not found"));
    }

    if (!vhdrEccCheck())
    {
        throw(EccException("ERROR: VHDR ECC check Failed"));
    }
}

auto IpzVpdParser::readTOC(types::BinaryVector::const_iterator& itrToVPD)
{
    // The offset to VTOC could be 1 or 2 bytes long
    uint16_t vtocOffset =
        readUInt16LE((itrToVPD + Offset::VTOC_PTR)); // itrToVPD);

    // Got the offset to VTOC, skip past record header and keyword header
    // to get to the record name.
    std::advance(itrToVPD, vtocOffset + sizeof(types::RecordId) +
                               sizeof(types::RecordSize) +
                               // Skip past the RT keyword, which contains
                               // the record name.
                               Length::KW_NAME + sizeof(types::KwSize));

    std::string record(itrToVPD, std::next(itrToVPD, Length::RECORD_NAME));
    if ("VTOC" != record)
    {
        throw(DataException("VTOC record not found"));
    }

    if (!vtocEccCheck())
    {
        throw(EccException("ERROR: VTOC ECC check Failed"));
    }

    // VTOC record name is good, now read through the TOC, stored in the PT
    // PT keyword; vpdBuffer is now pointing at the first character of the
    // name 'VTOC', jump to PT data.
    // Skip past record name and KW name, 'PT'
    std::advance(itrToVPD, Length::RECORD_NAME + Length::KW_NAME);

    // Note size of PT
    auto ptLen = *itrToVPD;

    // Skip past PT size
    std::advance(itrToVPD, sizeof(types::KwSize));

    // length of PT keyword
    return ptLen;
}

std::pair<types::RecordOffsetList, types::InvalidRecordList>
    IpzVpdParser::readPT(types::BinaryVector::const_iterator& itrToPT,
                         auto ptLength)
{
    types::RecordOffsetList recordOffsets;

    // List of names of all invalid records found.
    types::InvalidRecordList l_invalidRecordList;

    auto end = itrToPT;
    std::advance(end, ptLength);

    // Look at each entry in the PT keyword. In the entry,
    // we care only about the record offset information.
    while (itrToPT < end)
    {
        std::string recordName(itrToPT, itrToPT + Length::RECORD_NAME);
        // Skip record name and record type
        std::advance(itrToPT, Length::RECORD_NAME + sizeof(types::RecordType));

        // Get record offset
        recordOffsets.push_back(readUInt16LE(itrToPT));
        try
        {
            // Verify the ECC for this Record
            if (!recordEccCheck(itrToPT))
            {
                throw(EccException("ERROR: ECC check failed"));
            }
        }
        catch (const std::exception& l_ex)
        {
            logging::logMessage(l_ex.what());

            // add the invalid record name and exception object to list
            l_invalidRecordList.emplace_back(types::InvalidRecordEntry{
                recordName, EventLogger::getErrorType(l_ex)});
        }

        // Jump record size, record length, ECC offset and ECC length
        std::advance(itrToPT,
                     sizeof(types::RecordOffset) + sizeof(types::RecordLength) +
                         sizeof(types::ECCOffset) + sizeof(types::ECCLength));
    }

    return std::make_pair(recordOffsets, l_invalidRecordList);
}

types::IPZVpdMap::mapped_type IpzVpdParser::readKeywords(
    types::BinaryVector::const_iterator& itrToKwds)
{
    types::IPZVpdMap::mapped_type kwdValueMap{};
    while (true)
    {
        // Note keyword name
        std::string kwdName(itrToKwds, itrToKwds + Length::KW_NAME);
        if (constants::LAST_KW == kwdName)
        {
            // We're done
            break;
        }
        // Check if the Keyword is '#kw'
        char kwNameStart = *itrToKwds;

        // Jump past keyword name
        std::advance(itrToKwds, Length::KW_NAME);

        std::size_t kwdDataLength;
        std::size_t lengthHighByte;

        if (constants::POUND_KW == kwNameStart)
        {
            // Note keyword data length
            kwdDataLength = *itrToKwds;
            lengthHighByte = *(itrToKwds + 1);
            kwdDataLength |= (lengthHighByte << 8);

            // Jump past 2Byte keyword length
            std::advance(itrToKwds, sizeof(types::PoundKwSize));
        }
        else
        {
            // Note keyword data length
            kwdDataLength = *itrToKwds;

            // Jump past keyword length
            std::advance(itrToKwds, sizeof(types::KwSize));
        }

        // support all the Keywords
        auto stop = std::next(itrToKwds, kwdDataLength);
        std::string kwdata(itrToKwds, stop);
        kwdValueMap.emplace(std::move(kwdName), std::move(kwdata));

        // Jump past keyword data length
        std::advance(itrToKwds, kwdDataLength);
    }

    return kwdValueMap;
}

void IpzVpdParser::processRecord(auto recordOffset)
{
    // Jump to record name
    auto recordNameOffset =
        recordOffset + sizeof(types::RecordId) + sizeof(types::RecordSize) +
        // Skip past the RT keyword, which contains
        // the record name.
        Length::KW_NAME + sizeof(types::KwSize);

    // Get record name
    auto itrToVPDStart = m_vpdVector.cbegin();
    std::advance(itrToVPDStart, recordNameOffset);

    std::string recordName(itrToVPDStart, itrToVPDStart + Length::RECORD_NAME);

    // proceed to find contained keywords and their values.
    std::advance(itrToVPDStart, Length::RECORD_NAME);

    // Reverse back to RT Kw, in ipz vpd, to Read RT KW & value
    std::advance(itrToVPDStart, -(Length::KW_NAME + sizeof(types::KwSize) +
                                  Length::RECORD_NAME));

    // Add entry for this record (and contained keyword:value pairs)
    // to the parsed vpd output.
    m_parsedVPDMap.emplace(std::move(recordName),
                           std::move(readKeywords(itrToVPDStart)));
}

types::VPDMapVariant IpzVpdParser::parse()
{
    try
    {
        auto itrToVPD = m_vpdVector.cbegin();

        // Check vaidity of VHDR record
        checkHeader(itrToVPD);

        // Read the table of contents
        auto ptLen = readTOC(itrToVPD);

        // Read the table of contents record, to get offsets
        // to other records.
        auto l_result = readPT(itrToVPD, ptLen);
        auto recordOffsets = l_result.first;
        for (const auto& offset : recordOffsets)
        {
            processRecord(offset);
        }

        if (!processInvalidRecords(l_result.second))
        {
            logging::logMessage("Failed to process invalid records for [" +
                                m_vpdFilePath + "]");
        }

        return m_parsedVPDMap;
    }
    catch (const std::exception& e)
    {
        logging::logMessage(e.what());
        throw e;
    }
}

types::BinaryVector IpzVpdParser::getKeywordValueFromRecord(
    const types::Record& i_recordName, const types::Keyword& i_keywordName,
    const types::RecordOffset& i_recordDataOffset)
{
    auto l_iterator = m_vpdVector.cbegin();

    // Go to the record name in the given record's offset
    std::ranges::advance(l_iterator,
                         i_recordDataOffset + Length::JUMP_TO_RECORD_NAME,
                         m_vpdVector.cend());

    // Check if the record is present in the given record's offset
    if (i_recordName !=
        std::string(l_iterator,
                    std::ranges::next(l_iterator, Length::RECORD_NAME,
                                      m_vpdVector.cend())))
    {
        throw std::runtime_error(
            "Given record is not present in the offset provided");
    }

    std::ranges::advance(l_iterator, Length::RECORD_NAME, m_vpdVector.cend());

    std::string l_kwName = std::string(
        l_iterator,
        std::ranges::next(l_iterator, Length::KW_NAME, m_vpdVector.cend()));

    // Iterate through the keywords until the last keyword PF is found.
    while (l_kwName != constants::LAST_KW)
    {
        // First character required for #D keyword check
        char l_kwNameStart = *l_iterator;

        std::ranges::advance(l_iterator, Length::KW_NAME, m_vpdVector.cend());

        // Get the keyword's data length
        auto l_kwdDataLength = 0;

        if (constants::POUND_KW == l_kwNameStart)
        {
            l_kwdDataLength = readUInt16LE(l_iterator);
            std::ranges::advance(l_iterator, sizeof(types::PoundKwSize),
                                 m_vpdVector.cend());
        }
        else
        {
            l_kwdDataLength = *l_iterator;
            std::ranges::advance(l_iterator, sizeof(types::KwSize),
                                 m_vpdVector.cend());
        }

        if (l_kwName == i_keywordName)
        {
            // Return keyword's value to the caller
            return types::BinaryVector(
                l_iterator, std::ranges::next(l_iterator, l_kwdDataLength,
                                              m_vpdVector.cend()));
        }

        // next keyword search
        std::ranges::advance(l_iterator, l_kwdDataLength, m_vpdVector.cend());

        // next keyword name
        l_kwName = std::string(
            l_iterator,
            std::ranges::next(l_iterator, Length::KW_NAME, m_vpdVector.cend()));
    }

    // Keyword not found
    throw std::runtime_error("Given keyword not found.");
}

types::RecordData IpzVpdParser::getRecordDetailsFromVTOC(
    const types::Record& i_recordName, const types::RecordOffset& i_vtocOffset)
{
    // Get VTOC's PT keyword value.
    const auto l_vtocPTKwValue =
        getKeywordValueFromRecord("VTOC", "PT", i_vtocOffset);

    // Parse through VTOC PT keyword value to find the record which we are
    // interested in.
    auto l_vtocPTItr = l_vtocPTKwValue.cbegin();

    types::RecordData l_recordData;

    while (l_vtocPTItr < l_vtocPTKwValue.cend())
    {
        if (i_recordName ==
            std::string(l_vtocPTItr, l_vtocPTItr + Length::RECORD_NAME))
        {
            // Record found in VTOC PT keyword. Get offset
            std::ranges::advance(l_vtocPTItr,
                                 Length::RECORD_NAME + Length::RECORD_TYPE,
                                 l_vtocPTKwValue.cend());
            const auto l_recordOffset = readUInt16LE(l_vtocPTItr);

            std::ranges::advance(l_vtocPTItr, Length::RECORD_OFFSET,
                                 l_vtocPTKwValue.cend());
            const auto l_recordLength = readUInt16LE(l_vtocPTItr);

            std::ranges::advance(l_vtocPTItr, Length::RECORD_LENGTH,
                                 l_vtocPTKwValue.cend());
            const auto l_eccOffset = readUInt16LE(l_vtocPTItr);

            std::ranges::advance(l_vtocPTItr, Length::RECORD_ECC_OFFSET,
                                 l_vtocPTKwValue.cend());
            const auto l_eccLength = readUInt16LE(l_vtocPTItr);

            l_recordData = std::make_tuple(l_recordOffset, l_recordLength,
                                           l_eccOffset, l_eccLength);
            break;
        }

        std::ranges::advance(l_vtocPTItr, Length::SKIP_A_RECORD_IN_PT,
                             l_vtocPTKwValue.cend());
    }

    return l_recordData;
}

types::DbusVariantType IpzVpdParser::readKeywordFromHardware(
    const types::ReadVpdParams i_paramsToReadData)
{
    // Extract record and keyword from i_paramsToReadData
    types::Record l_record;
    types::Keyword l_keyword;

    if (const types::IpzType* l_ipzData =
            std::get_if<types::IpzType>(&i_paramsToReadData))
    {
        l_record = std::get<0>(*l_ipzData);
        l_keyword = std::get<1>(*l_ipzData);
    }
    else
    {
        logging::logMessage(
            "Input parameter type provided isn't compatible with the given VPD type.");
        throw types::DbusInvalidArgument();
    }

    // Read keyword's value from vector
    auto l_itrToVPD = m_vpdVector.cbegin();

    if (l_record == "VHDR")
    {
// Disable providing a way to read keywords from VHDR for the time being.
#if 0
        std::ranges::advance(l_itrToVPD, Offset::VHDR_RECORD,
                             m_vpdVector.cend());

        return types::DbusVariantType{getKeywordValueFromRecord(
            l_record, l_keyword, Offset::VHDR_RECORD)};
#endif

        logging::logMessage("Read cannot be performed on VHDR record.");
        throw types::DbusInvalidArgument();
    }

    // Get VTOC offset
    std::ranges::advance(l_itrToVPD, Offset::VTOC_PTR, m_vpdVector.cend());
    auto l_vtocOffset = readUInt16LE(l_itrToVPD);

    if (l_record == "VTOC")
    {
        // Disable providing a way to read keywords from VTOC for the time
        // being.
#if 0
        return types::DbusVariantType{
            getKeywordValueFromRecord(l_record, l_keyword, l_vtocOffset)};
#endif

        logging::logMessage("Read cannot be performed on VTOC record.");
        throw types::DbusInvalidArgument();
    }

    // Get record offset from VTOC's PT keyword value.
    auto l_recordData = getRecordDetailsFromVTOC(l_record, l_vtocOffset);
    const auto l_recordOffset = std::get<0>(l_recordData);

    if (l_recordOffset == 0)
    {
        throw std::runtime_error("Record not found in VTOC PT keyword.");
    }

    // Get the given keyword's value
    return types::DbusVariantType{
        getKeywordValueFromRecord(l_record, l_keyword, l_recordOffset)};
}

void IpzVpdParser::updateRecordECC(
    const auto& i_recordDataOffset, const auto& i_recordDataLength,
    const auto& i_recordECCOffset, size_t i_recordECCLength,
    types::BinaryVector& io_vpdVector)
{
    auto l_recordDataBegin =
        std::next(io_vpdVector.begin(), i_recordDataOffset);

    auto l_recordECCBegin = std::next(io_vpdVector.begin(), i_recordECCOffset);

    auto l_eccStatus = vpdecc_create_ecc(
        const_cast<uint8_t*>(&l_recordDataBegin[0]), i_recordDataLength,
        const_cast<uint8_t*>(&l_recordECCBegin[0]), &i_recordECCLength);

    if (l_eccStatus != VPD_ECC_OK)
    {
        throw(EccException(
            "ECC update failed with error " + std::to_string(l_eccStatus)));
    }

    auto l_recordECCEnd = std::next(l_recordECCBegin, i_recordECCLength);

    m_vpdFileStream.seekp(m_vpdStartOffset + i_recordECCOffset, std::ios::beg);

    std::copy(l_recordECCBegin, l_recordECCEnd,
              std::ostreambuf_iterator<char>(m_vpdFileStream));
}

int IpzVpdParser::setKeywordValueInRecord(
    const types::Record& i_recordName, const types::Keyword& i_keywordName,
    const types::BinaryVector& i_keywordData,
    const types::RecordOffset& i_recordDataOffset,
    types::BinaryVector& io_vpdVector)
{
    auto l_iterator = io_vpdVector.begin();

    // Go to the record name in the given record's offset
    std::ranges::advance(l_iterator,
                         i_recordDataOffset + Length::JUMP_TO_RECORD_NAME,
                         io_vpdVector.end());

    const std::string l_recordFound(
        l_iterator,
        std::ranges::next(l_iterator, Length::RECORD_NAME, io_vpdVector.end()));

    // Check if the record is present in the given record's offset
    if (i_recordName != l_recordFound)
    {
        throw(DataException("Given record found at the offset " +
                            std::to_string(i_recordDataOffset) + " is : " +
                            l_recordFound + " and not " + i_recordName));
    }

    std::ranges::advance(l_iterator, Length::RECORD_NAME, io_vpdVector.end());

    std::string l_kwName = std::string(
        l_iterator,
        std::ranges::next(l_iterator, Length::KW_NAME, io_vpdVector.end()));

    // Iterate through the keywords until the last keyword PF is found.
    while (l_kwName != constants::LAST_KW)
    {
        // First character required for #D keyword check
        char l_kwNameStart = *l_iterator;

        std::ranges::advance(l_iterator, Length::KW_NAME, io_vpdVector.end());

        // Find the keyword's data length
        size_t l_kwdDataLength = 0;

        if (constants::POUND_KW == l_kwNameStart)
        {
            l_kwdDataLength = readUInt16LE(l_iterator);
            std::ranges::advance(l_iterator, sizeof(types::PoundKwSize),
                                 io_vpdVector.end());
        }
        else
        {
            l_kwdDataLength = *l_iterator;
            std::ranges::advance(l_iterator, sizeof(types::KwSize),
                                 io_vpdVector.end());
        }

        if (l_kwName == i_keywordName)
        {
            // Before writing the keyword's value, get the maximum size that can
            // be updated.
            const auto l_lengthToUpdate =
                i_keywordData.size() <= l_kwdDataLength
                    ? i_keywordData.size()
                    : l_kwdDataLength;

            // Set the keyword's value on vector. This is required to update the
            // record's ECC based on the new value set.
            const auto i_keywordDataEnd = std::ranges::next(
                i_keywordData.cbegin(), l_lengthToUpdate, i_keywordData.cend());

            std::copy(i_keywordData.cbegin(), i_keywordDataEnd, l_iterator);

            // Set the keyword's value on hardware
            const auto l_kwdDataOffset =
                std::distance(io_vpdVector.begin(), l_iterator);
            m_vpdFileStream.seekp(m_vpdStartOffset + l_kwdDataOffset,
                                  std::ios::beg);

            std::copy(i_keywordData.cbegin(), i_keywordDataEnd,
                      std::ostreambuf_iterator<char>(m_vpdFileStream));

            // return no of bytes set
            return l_lengthToUpdate;
        }

        // next keyword search
        std::ranges::advance(l_iterator, l_kwdDataLength, io_vpdVector.end());

        // next keyword name
        l_kwName = std::string(
            l_iterator,
            std::ranges::next(l_iterator, Length::KW_NAME, io_vpdVector.end()));
    }

    // Keyword not found
    throw(DataException(
        "Keyword " + i_keywordName + " not found in record " + i_recordName));
}

int IpzVpdParser::writeKeywordOnHardware(
    const types::WriteVpdParams i_paramsToWriteData)
{
    int l_sizeWritten = -1;

    try
    {
        types::Record l_recordName;
        types::Keyword l_keywordName;
        types::BinaryVector l_keywordData;

        // Extract record, keyword and value from i_paramsToWriteData
        if (const types::IpzData* l_ipzData =
                std::get_if<types::IpzData>(&i_paramsToWriteData))
        {
            l_recordName = std::get<0>(*l_ipzData);
            l_keywordName = std::get<1>(*l_ipzData);
            l_keywordData = std::get<2>(*l_ipzData);
        }
        else
        {
            logging::logMessage(
                "Input parameter type provided isn't compatible with the given FRU's VPD type.");
            throw types::DbusInvalidArgument();
        }

        if (l_recordName == "VHDR" || l_recordName == "VTOC")
        {
            logging::logMessage(
                "Write operation not allowed on the given record : " +
                l_recordName);
            throw types::DbusNotAllowed();
        }

        if (l_keywordData.size() == 0)
        {
            logging::logMessage(
                "Write operation not allowed as the given keyword's data length is 0.");
            throw types::DbusInvalidArgument();
        }

        auto l_vpdBegin = m_vpdVector.begin();

        // Get VTOC offset
        std::ranges::advance(l_vpdBegin, Offset::VTOC_PTR, m_vpdVector.end());
        auto l_vtocOffset = readUInt16LE(l_vpdBegin);

        // Get the details of user given record from VTOC
        const types::RecordData& l_inputRecordDetails =
            getRecordDetailsFromVTOC(l_recordName, l_vtocOffset);

        const auto& l_inputRecordOffset = std::get<0>(l_inputRecordDetails);

        if (l_inputRecordOffset == 0)
        {
            throw(DataException("Record not found in VTOC PT keyword."));
        }

        // Create a local copy of m_vpdVector to perform keyword update and ecc
        // update on filestream.
        types::BinaryVector l_vpdVector = m_vpdVector;

        // write keyword's value on hardware
        l_sizeWritten =
            setKeywordValueInRecord(l_recordName, l_keywordName, l_keywordData,
                                    l_inputRecordOffset, l_vpdVector);

        if (l_sizeWritten <= 0)
        {
            throw(DataException("Unable to set value on " + l_recordName + ":" +
                                l_keywordName));
        }

        // Update the record's ECC
        updateRecordECC(l_inputRecordOffset, std::get<1>(l_inputRecordDetails),
                        std::get<2>(l_inputRecordDetails),
                        std::get<3>(l_inputRecordDetails), l_vpdVector);

        logging::logMessage(std::to_string(l_sizeWritten) +
                            " bytes updated successfully on hardware for " +
                            l_recordName + ":" + l_keywordName);
    }
    catch (const std::exception& l_exception)
    {
        throw;
    }

    return l_sizeWritten;
}

bool IpzVpdParser::processInvalidRecords(
    const types::InvalidRecordList& i_invalidRecordList) const noexcept
{
    bool l_rc{true};
    if (!i_invalidRecordList.empty())
    {
        auto l_invalidRecordToString =
            [](const types::InvalidRecordEntry& l_record) {
                return std::string{
                    "{" + l_record.first + "," +
                    EventLogger::getErrorTypeString(l_record.second) + "}"};
            };

        std::string l_invalidRecordListString{"["};
        try
        {
            for (const auto& l_entry : i_invalidRecordList)
            {
                l_invalidRecordListString +=
                    l_invalidRecordToString(l_entry) + ",";
            }
            l_invalidRecordListString += "]";
        }
        catch (const std::exception& l_ex)
        {
            l_invalidRecordListString = "";
        }

        // Log a Predictive PEL, including names and respective error messages
        // of all invalid records
        EventLogger::createSyncPelWithInvCallOut(
            types::ErrorType::VpdParseError, types::SeverityType::Warning,
            __FILE__, __FUNCTION__, constants::VALUE_0,
            std::string(
                "Check failed for record(s) while parsing VPD. Check user data for reason and list of failed record(s). Re-program VPD."),
            std::vector{
                std::make_tuple(m_vpdFilePath, types::CalloutPriority::High)},
            l_invalidRecordListString, std::nullopt, std::nullopt,
            std::nullopt);

        uint16_t l_errCode = 0;

        // Dump Bad VPD to file
        if (constants::SUCCESS != vpdSpecificUtility::dumpBadVpd(
                                      m_vpdFilePath, m_vpdVector, l_errCode))
        {
            if (l_errCode)
            {
                logging::logMessage("Failed to dump bad vpd file. Error : " +
                                    commonUtility::getErrCodeMsg(l_errCode));
            }

            l_rc = false;
        }
    }
    return l_rc;
}
} // namespace vpd
