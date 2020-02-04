#include "editor.hpp"

#include "utils.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

#include "vpdecc/vpdecc.h"

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{

void Editor::checkPTForRecord(RecordOffset ptOffset, std::size_t ptLength)
{
    vpdFileStream.seekg(ptOffset);
    Binary PTrecord(ptLength);

    vpdFileStream.read(reinterpret_cast<char*>(PTrecord.data()), ptLength);

    auto iterator = PTrecord.cbegin();
    auto end = PTrecord.cend();

    // Look at each entry in the PT keyword for the record name
    while (iterator < end)
    {
        auto stop = std::next(iterator, lengths::RECORD_NAME);
        std::string record(iterator, stop);

        if (record == thisRecord.recName)
        {
            // Skip record name and record type
            std::advance(iterator, lengths::RECORD_NAME + sizeof(RecordType));

            // Get record offset
            thisRecord.recOffset = readUInt16LE(iterator);

            // pass the record offset length to read record length
            std::advance(iterator, lengths::RECORD_OFFSET);
            thisRecord.recSize = readUInt16LE(iterator);

            std::advance(iterator, lengths::RECORD_LENGTH);
            thisRecord.recECCoffset = readUInt16LE(iterator);

            ECCLength len;
            std::advance(iterator, lengths::RECORD_ECC_OFFSET);
            len = readUInt16LE(iterator);
            thisRecord.recECCLength = len;

            // once we find the record we don't need to look further
            return;
        }
        else
        {
            // Jump the record
            std::advance(iterator, lengths::RECORD_NAME + sizeof(RecordType) +
                                       sizeof(RecordSize) +
                                       sizeof(RecordLength) +
                                       sizeof(ECCOffset) + sizeof(ECCLength));
        }
    }

    // imples the record was not found
    throw std::runtime_error("Record not found");
}

void Editor::updateData(Binary kwdData)
{
    std::size_t lengthToUpdate = kwdData.size() <= thisRecord.KwdDataLength
                                     ? kwdData.size()
                                     : thisRecord.KwdDataLength;

    auto iteratorToNewdata = kwdData.cbegin();
    auto end = iteratorToNewdata;
    std::advance(end, lengthToUpdate);

    std::copy(iteratorToNewdata, end,
              std::ostreambuf_iterator<char>(vpdFileStream));
}

void Editor::checkRecordForKwd()
{
    RecordOffset recOffset = thisRecord.recOffset;

    // Jump to record name
    auto nameOffset = recOffset + sizeof(RecordId) + sizeof(RecordSize) +
                      // Skip past the RT keyword, which contains
                      // the record name.
                      lengths::KW_NAME + sizeof(KwSize);

    vpdFileStream.seekg(nameOffset + lengths::RECORD_NAME, std::ios::beg);

    (thisRecord.recData).resize(thisRecord.recSize);
    vpdFileStream.read(reinterpret_cast<char*>((thisRecord.recData).data()),
                       thisRecord.recSize);

    auto iterator = (thisRecord.recData).cbegin();
    auto end = (thisRecord.recData).cend();

    std::size_t dataLength;
    while (iterator < end)
    {
        // Note keyword name
        std::string kw(iterator, iterator + lengths::KW_NAME);

        // Check if the Keyword starts with '#'
        char kwNameStart = *iterator;
        std::advance(iterator, lengths::KW_NAME);

        // if keyword starts with #
        if (POUND_KW == kwNameStart)
        {
            // Note existing keyword data length
            dataLength = readUInt16LE(iterator);

            // Jump past 2Byte keyword length + data
            std::advance(iterator, sizeof(PoundKwSize));
        }
        else
        {
            // Note existing keyword data length
            dataLength = *iterator;

            // Jump past keyword length and data
            std::advance(iterator, sizeof(KwSize));
        }

        if (thisRecord.recKWd == kw)
        {
            // We're done
            std::size_t kwdOffset =
                std::distance((thisRecord.recData).cbegin(), iterator);
            vpdFileStream.seekp(nameOffset + lengths::RECORD_NAME + kwdOffset,
                                std::ios::beg);
            thisRecord.KwdDataLength = dataLength;

            return;
        }

        // jump the data of current kwd to point to next kwd name
        std::advance(iterator, dataLength);
    }

    throw std::runtime_error("Keyword not found");
}

void Editor::updateRecordECC()
{
    vpdFileStream.seekp(thisRecord.recECCoffset, std::ios::beg);

    (thisRecord.recEccData).resize(thisRecord.recECCLength);
    vpdFileStream.read(reinterpret_cast<char*>((thisRecord.recEccData).data()),
                       thisRecord.recECCLength);

    auto recPtr = (thisRecord.recData).cbegin();
    auto recEccPtr = (thisRecord.recEccData).cbegin();

    auto l_status = vpdecc_create_ecc(
        const_cast<uint8_t*>(&recPtr[0]), thisRecord.recSize,
        const_cast<uint8_t*>(&recEccPtr[0]), &thisRecord.recECCLength);
    if (l_status != VPD_ECC_OK)
    {
        throw std::runtime_error("Ecc update failed");
    }

    auto end = (thisRecord.recEccData).cbegin();
    std::advance(end, thisRecord.recECCLength);

    std::copy((thisRecord.recEccData).cbegin(), end,
              std::ostreambuf_iterator<char>(vpdFileStream));
}

void Editor::updateKeyword(std::tuple<RecordOffset, std::size_t> ptInfo,
                           Binary kwdData)
{
    vpdFileStream.open(vpdFilePath,
                       std::ios::in | std::ios::out | std::ios::binary);
    if (!vpdFileStream)
    {
        throw std::runtime_error("unable to open vpd file to edit");
    }

    // search PT for record name
    checkPTForRecord(std::get<0>(ptInfo), std::get<1>(ptInfo));

    // check record for keywrod
    checkRecordForKwd();

    // update the data to the file
    updateData(kwdData);

    // update the ECC data for the record once data has been updated
    updateRecordECC();
}

} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
