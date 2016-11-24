#include <exception>
#include <iostream>
#include <unordered_map>
#include <defines.hpp>
#include "impl.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{

static const std::unordered_map<std::string, Record> supportedRecords = {
    {"VINI", Record::VINI},
    {"OPFR", Record::OPFR},
    {"OSYS", Record::OSYS}
};

using RecordId = uint8_t;
using RecordOffset = uint16_t;
using RecordSize = uint16_t;
using RecordType = uint16_t;
using RecordLength = uint16_t;
using KwSize = uint8_t;
using KwLength = uint32_t;
using ECCOffset = uint16_t;
using ECCLength = uint16_t;

namespace offsets
{

enum Offsets
{
    VHDR = 17,
    VHDR_TOC_ENTRY = 29,
    VTOC_PTR = 35,
};

}

namespace lengths
{

enum Lengths
{
    RECORD_NAME = 4,
    KW_NAME = 2,
    RECORD_MIN = 44,
};

}

void Impl::checkHeader() const
{
    if(_vpd.empty() || (lengths::RECORD_MIN > _vpd.size()))
    {
        throw std::runtime_error("Malformed VPD");
    }
    else
    {
        auto buf = _vpd.data() + offsets::VHDR;
        std::string record(buf, lengths::RECORD_NAME);
        if("VHDR" != record)
        {
            throw std::runtime_error("VHDR record not found");
        }
    }
}

OffsetList Impl::readTOC() const
{
    OffsetList offsets {};

    // The offset to VTOC could be 1 or 2 bytes long
    RecordOffset vtocOft = _vpd.at(offsets::VTOC_PTR);
    RecordOffset highByte = _vpd.at(offsets::VTOC_PTR + 1);
    if(highByte)
    {
        // Offset is 2 bytes long
        vtocOft = (highByte << 8) |
                  vtocOft;
    }

    // Got the offset to VTOC, skip past record header and keyword header
    // to get to the record name.
    auto vpdBuffer = _vpd.data() +
                     vtocOft +
                     sizeof(RecordId) +
                     sizeof(RecordSize) +
                     // Skip past the RT keyword, which contains
                     // the record name.
                     lengths::KW_NAME +
                     sizeof(KwSize);

    std::string record(vpdBuffer, lengths::RECORD_NAME);
    if("VTOC" != record)
    {
        throw std::runtime_error("VTOC record not found");
    }

    // VTOC record name is good, now read through the TOC,
    // stored in the PT keyword; vpdBuffer is now pointing at
    // the first character of the name 'VTOC', jump to PT
    // data.
    vpdBuffer += (lengths::RECORD_NAME);
    // Skip past KW name, 'PT'
    vpdBuffer += lengths::KW_NAME;
    // Note size of PT
    KwLength ptLen = (*vpdBuffer);
    // Skip past PT size
    vpdBuffer += sizeof(KwSize);
    // vpdBuffer is now pointing to PT data
    offsets = readPT(vpdBuffer, ptLen);

    return offsets;
}

OffsetList Impl::readPT(const Byte* vpdBuffer, std::size_t ptLength) const
{
    OffsetList offsets{};

    auto end = vpdBuffer + ptLength;

    // Look at each entry in the PT keyword. In the entry,
    // we care only about the record offset information.
    while(vpdBuffer < end)
    {
        // Skip record name
        vpdBuffer += lengths::RECORD_NAME;
        // Skip record type
        vpdBuffer += sizeof(RecordType);

        // Get record offset
        RecordOffset recOft = *vpdBuffer;
        RecordOffset highByte = *(vpdBuffer + 1);
        if(highByte)
        {
            // Offset is 2 bytes long
            recOft = (highByte << 8) |
                     recOft;
        }
        offsets.push_back(recOft);

        // Jump record size
        vpdBuffer += sizeof(RecordSize);
        // Skip record length
        vpdBuffer += sizeof(RecordLength);
        // Skip ECC offset
        vpdBuffer += sizeof(ECCOffset);
        // Skip ECC length
        vpdBuffer += sizeof(ECCLength);
    }

    return offsets;
}

void Impl::processRecord(std::size_t recordOffset)
{
    // Jump to record name
    auto nameOffset = recordOffset +
                   sizeof(RecordId) +
                   sizeof(RecordSize) +
                   // Skip past the RT keyword, which contains
                   // the record name.
                   lengths::KW_NAME +
                   sizeof(KwSize);
    // Get record name
    auto vpdBufferPtr = _vpd.data() + nameOffset;

    std::string name(vpdBufferPtr, lengths::RECORD_NAME);
    if(supportedRecords.end() != supportedRecords.find(name))
    {
        // If it's a record we're interested in, proceed to find
        // contained keywords and their values.
        auto kwMap = readKeywords(nameOffset + lengths::RECORD_NAME);
        // Add entry for this record (and contained keyword:value pairs)
        // to the parsed vpd output.
        _out.emplace(std::make_pair(name, kwMap));
    }
}

} // namespace parser
} // namespace vpd
} // namespace openpower
