#include <exception>
#include <iostream>
#include "impl.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{

using RecordOffset = uint16_t;
using RecordSize = uint16_t;
using RecordType = uint16_t;
using RecordLength = uint16_t;
using KwSize = uint8_t;
using KwLen = uint32_t;
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
    RECORD_ID = 1,
    KW_NAME = 2,
};

}

void Impl::hasHeader() const
{
    auto buf = _vpd.data() + offsets::VHDR;
    std::string record(buf, lengths::RECORD_NAME);
    if("VHDR" != record)
    {
        throw std::runtime_error("VHDR record not found");
    }
}

OffsetList Impl::readTOC() const
{
    OffsetList ofts{};

    // The offset to VTOC could be 1 or 2 bytes long
    RecordOffset vtocOft = _vpd.at(offsets::VTOC_PTR);
    RecordOffset lowByte = _vpd.at(offsets::VTOC_PTR + 1);
    if(lowByte)
    {
        // Offset is 2 bytes long
        vtocOft = (vtocOft << 8) |
                   lowByte;
    }

    // Got the offset to VTOC, skip past record header and keyword header
    // to get to the record name.
    auto buf = _vpd.data() +
               vtocOft +
               lengths::RECORD_ID +
               sizeof(RecordSize) +
               lengths::KW_NAME +
               sizeof(KwSize);
    std::string record(buf, lengths::RECORD_NAME);
    if("VTOC" != record)
    {
        throw std::runtime_error("VTOC record not found");
    }

    // VTOC record name is good, now read through the TOC,
    // stored in the PT keyword; buf is now pointing at
    // the first character of the name 'VTOC', jump to PT
    // data.
    buf += (lengths::RECORD_NAME);
    // Skip past KW name, 'PT'
    buf += lengths::KW_NAME;
    // Note size of PT
    KwLen ptLen = (*buf);
    // Skip past PT size
    buf += sizeof(KwSize);
    ofts = readPT(buf, ptLen);

    return ofts;
}

OffsetList Impl::readPT(auto buf, auto len) const
{
    OffsetList ofts{};

    uint32_t ptOft = 0;

    // Look at each entry in the PT keyword. In the entry,
    // we care only about the record offset information.
    while(ptOft < len)
    {
        // Skip record name
        ptOft += lengths::RECORD_NAME;
        buf += lengths::RECORD_NAME;
        // Skip record type
        ptOft += sizeof(RecordType);
        buf += sizeof(RecordType);
        RecordOffset recOft = *buf;
        RecordOffset lowByte = *(buf + 1);
        if(lowByte)
        {
            // Offset is 2 bytes long
            recOft = (recOft << 8) |
                     lowByte;
        }
        ofts.push_back(recOft);
        // Jump record size
        ptOft += sizeof(RecordSize);
        buf += sizeof(RecordSize);
        // Skip record length
        ptOft += sizeof(RecordSize);
        buf += sizeof(RecordSize);
        // Skip ECC offset
        ptOft += sizeof(ECCOffset);
        buf += sizeof(ECCOffset);
        // Skip ECC length
        ptOft += sizeof(ECCLength);
        buf += sizeof(ECCLength);
    }

    return ofts;
}

} // namespace parser
} // namespace vpd
} // namespace openpower
