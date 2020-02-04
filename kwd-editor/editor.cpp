#include "editor.hpp"

#include "createEcc.hpp"

#include <iostream>

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{
static constexpr auto LAST_KW = "PF";
static constexpr auto POUND_KW = '#';

namespace lengths
{
enum Lengths
{
    RECORD_NAME = 4,
    KW_NAME = 2,
    RECORD_MIN = 44,
};
}

namespace offsets
{
enum Offsets
{
    VHDR = 17,
    VHDR_TOC_ENTRY = 29,
    VTOC_PTR = 35,
};
}

RecordOffset Editor::checkPTForRecord(Binary::const_iterator& iterator,
                                      std::size_t ptLength,
                                      const std::string recName)
{
    // holds the offset of record
    RecordOffset offset;

    auto end = iterator;
    std::advance(end, ptLength);

    // Look at each entry in the PT keyword for the record name
    while (iterator < end)
    {
        auto stop = std::next(iterator, lengths::RECORD_NAME);
        std::string record(iterator, stop);

        if (record == recName)
        {
            // Skip record name and record type
            std::advance(iterator, lengths::RECORD_NAME + sizeof(RecordType));

            // Get record offset
            offset = *iterator;
            RecordOffset highByte = *(iterator + 1);
            offset |= (highByte << 8);

            // we don't need to look once the record is found
            return offset;
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

void Editor::updateData(Binary kwdData, std::size_t kwddataLength,
                        Binary::iterator& iterator)
{
    std::size_t lengthToUpdate =
        kwdData.size() <= kwddataLength ? kwdData.size() : kwddataLength;

    auto iteratorToNewdata = kwdData.cbegin();

    while (lengthToUpdate)
    {
        *iterator = *iteratorToNewdata;
        std::advance(iterator, sizeof(KwSize));
        std::advance(iteratorToNewdata, sizeof(KwSize));
        lengthToUpdate--;
    }
}

void Editor::checkRecordForKwd(RecordOffset recOffset,
                               const std::string kwdName, Binary kwdData)
{
    // Jump to record name
    auto nameOffset = recOffset + sizeof(RecordId) + sizeof(RecordSize) +
                      // Skip past the RT keyword, which contains
                      // the record name.
                      lengths::KW_NAME + sizeof(KwSize);

    auto iterator = vpd.begin();

    // skip the record name
    std::advance(iterator, nameOffset + lengths::RECORD_NAME);

    std::size_t dataLength;
    std::size_t dataLengthHighByte;

    while (true)
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
            dataLength = *iterator;
            dataLengthHighByte = *(iterator + 1);
            dataLength |= (dataLengthHighByte << 8);

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

        // std::cout<<"Start the loop"<<std::endl;
        if (kwdName == kw)
        {
            // We're done
            break;
        }
        else if (LAST_KW == kw)
        {
            // kwd not found
            throw std::runtime_error("Keyword not found");
        }

        // jump the data of current kwd to point to next kwd name
        std::advance(iterator, dataLength);
    }

    // update kwd data
    updateData(kwdData, dataLength, iterator);
}

void Editor::updateKeyword(Binary::const_iterator& iterator,
                           std::size_t ptLength, const std::string recName,
                           const std::string kwdName, Binary kwdData)
{
    // search PT for record name
    RecordOffset Rec_Offset = checkPTForRecord(iterator, ptLength, recName);

    // check record for keywrod
    checkRecordForKwd(Rec_Offset, kwdName, kwdData);

    // now the iterator points to the record offset
    openpower::vpd::modify::createWriteEccForThisRecord(std::move(vpd),
                                                        iterator);
}

} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
