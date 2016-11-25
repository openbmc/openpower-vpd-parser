#include <sstream>
#include <exception>
#include <iostream>
#include <unordered_map>
#include <iomanip>
#include <defines.hpp>
#include "impl.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{

static constexpr auto LAST_KW = "PF";
static constexpr auto MAC_ADDRESS_LEN_BYTES = 6;

static const std::unordered_map<std::string, Record> supportedRecords = {
    {"VINI", Record::VINI},
    {"OPFR", Record::OPFR},
    {"OSYS", Record::OSYS}
};

static const std::unordered_map<std::string,
                 KeywordInfo> supportedKeywords = {
    {"DR", {record::Keyword::DR, keyword::Encoding::ASCII}},
    {"PN", {record::Keyword::PN, keyword::Encoding::ASCII}},
    {"SN", {record::Keyword::SN, keyword::Encoding::ASCII}},
    {"CC", {record::Keyword::CC, keyword::Encoding::ASCII}},
    {"HW", {record::Keyword::HW, keyword::Encoding::HEX}},
    {"B1", {record::Keyword::B1, keyword::Encoding::B1}},
    {"VN", {record::Keyword::VN, keyword::Encoding::ASCII}},
    {"MB", {record::Keyword::MB, keyword::Encoding::HEX}},
    {"MM", {record::Keyword::MM, keyword::Encoding::ASCII}}
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

void Impl::hasHeader() const
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

OffsetList Impl::readPT(auto vpdBuffer, auto ptLength) const
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

void Impl::processRecord(auto recordOffset)
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

KeywordMap Impl::readKeywords(auto kwOft)
{
     auto vpdBufferPtr = _vpd.data() +
                         kwOft;

     // Now we are the keyword entries for
     // a record
     KeywordMap map {};
     while(true)
     {
         // Note keyword name
         std::string kw(vpdBufferPtr, lengths::KW_NAME);
         if(LAST_KW == kw)
         {
             // We're done
             break;
         }
         // Jump past keyword name
         vpdBufferPtr += lengths::KW_NAME;
         // Note keyword data length
         KwLength length = *vpdBufferPtr;
         // Jump past keyword length
         vpdBufferPtr += sizeof(KwSize);
         // Pointing to keyword data now
         if(supportedKeywords.end() != supportedKeywords.find(kw))
         {
             // Keyword is of interest to us
             auto kwDataOffset = vpdBufferPtr - _vpd.data();
             std::string data = readKwData(
                                     (supportedKeywords.find(kw))->second,
                                     length,
                                     kwDataOffset);
             map.emplace(std::make_pair(kw, data));
         }
         // Jump past keyword data length
         vpdBufferPtr += length;
     }

     return map;
}

std::string Impl::readKwData(const KeywordInfo& keyword,
                             auto dataLength,
                             auto dataOffset)
{
    std::string data {};
    std::stringstream ss {};

    switch(keyword.second)
    {
        case keyword::Encoding::ASCII:
        {
            data = std::move(
                        std::string(_vpd.data() + dataOffset,
                            dataLength));
            break;
        }

        case keyword::Encoding::HEX:
        {
            auto start = _vpd.data() + dataOffset;
            auto stop = start + dataLength;
            while(start < stop)
            {
                uint16_t digit = *start;
                ss << std::hex << std::setw(2) << std::setfill('0') << digit;
                ++start;
            }
            data = ss.str();
            break;
        }

        case keyword::Encoding::B1:
        {
            //B1 is MAC address, represent as AA::BB::CC::DD::EE::FF
            auto start = _vpd.data() + dataOffset;
            auto stop = start + MAC_ADDRESS_LEN_BYTES;
            while(start < stop)
            {
                uint16_t digit = *start;
                ss << std::hex << std::setw(2) << std::setfill('0') << digit;
                ++start;
                if(start < stop)
                {
                    ss << ":";
                }
            }
            data = ss.str();
            break;
        }

        default:
            break;
    }

    return data;
}

Store Impl::run()
{
    try
    {
        // Check if the VHDR record is present
        hasHeader();
        // Read the table of contents record, to get offsets
        // to other records.
        auto offsets = readTOC();
        for(auto& offset : offsets)
        {
            processRecord(offset);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
    }

    // Return a Store object, which has interfaces to
    // access parsed VPD by record:keyword
    Store s(std::move(_out));
    return s;
}

} // namespace parser
} // namespace vpd
} // namespace openpower
