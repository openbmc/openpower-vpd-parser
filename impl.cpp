#include <defines.hpp>
#include <exception>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include "impl.hpp"
#include "endian.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{

static constexpr auto LAST_KW = "PF";

static const std::unordered_map<std::string, Record> supportedRecords =
{
    {"VINI", Record::VINI},
    {"OPFR", Record::OPFR},
    {"OSYS", Record::OSYS}
};

static const std::unordered_map<std::string,
       record::Keyword> supportedKeywords =
{
    {"DR", record::Keyword::DR},
    {"PN", record::Keyword::PN},
    {"SN", record::Keyword::SN},
    {"CC", record::Keyword::CC},
    {"HW", record::Keyword::HW},
    {"B1", record::Keyword::B1},
    {"VN", record::Keyword::VN},
    {"MB", record::Keyword::MB},
    {"MM", record::Keyword::MM}
};

namespace
{

using RecordId = uint8_t;
using RecordOffset = uint16_t;
using RecordSize = uint16_t;
using RecordType = uint16_t;
using RecordLength = uint16_t;
using KwSize = uint8_t;
using ECCOffset = uint16_t;
using ECCLength = uint16_t;

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
    if (vpd.empty() || (lengths::RECORD_MIN > vpd.size()))
    {
        throw std::runtime_error("Malformed VPD");
    }
    else
    {
        auto iterator = vpd.cbegin();
        std::advance(iterator, offsets::VHDR);
        std::string record(iterator, iterator + lengths::RECORD_NAME);
        if ("VHDR" != record)
        {
            throw std::runtime_error("VHDR record not found");
        }
    }
}

OffsetList Impl::readTOC() const
{
    OffsetList offsets {};

    // The offset to VTOC could be 1 or 2 bytes long
    RecordOffset vtocOffset =
        endian::toNetwork<RecordOffset>((vpd.at(offsets::VTOC_PTR) << 8) |
                                        vpd.at(offsets::VTOC_PTR + 1));

    // Got the offset to VTOC, skip past record header and keyword header
    // to get to the record name.
    auto iterator = vpd.cbegin();
    std::advance(iterator,
                 vtocOffset +
                 sizeof(RecordId) +
                 sizeof(RecordSize) +
                 // Skip past the RT keyword, which contains
                 // the record name.
                 lengths::KW_NAME +
                 sizeof(KwSize));

    std::string record(iterator, iterator + lengths::RECORD_NAME);
    if ("VTOC" != record)
    {
        throw std::runtime_error("VTOC record not found");
    }

    // VTOC record name is good, now read through the TOC, stored in the PT
    // PT keyword; vpdBuffer is now pointing at the first character of the
    // name 'VTOC', jump to PT data.
    // Skip past record name and KW name, 'PT'
    std::advance(iterator, lengths::RECORD_NAME + lengths::KW_NAME);
    // Note size of PT
    std::size_t ptLen = *iterator;
    // Skip past PT size
    std::advance(iterator, sizeof(KwSize));

    // vpdBuffer is now pointing to PT data
    return readPT(iterator, ptLen);
}

OffsetList Impl::readPT(Binary::const_iterator iterator,
                        std::size_t ptLength) const
{
    OffsetList offsets{};

    auto end = iterator;
    std::advance(end, ptLength);

    // Look at each entry in the PT keyword. In the entry,
    // we care only about the record offset information.
    while (iterator < end)
    {
        // Skip record name and record type
        std::advance(iterator, lengths::RECORD_NAME + sizeof(RecordType));

        // Get record offset
        RecordOffset offset =
            endian::toNetwork<RecordOffset>(((*iterator) << 8) |
                                            *(iterator + 1));
        offsets.push_back(offset);

        // Jump record size, record length, ECC offset and ECC length
        std::advance(iterator,
                     sizeof(RecordSize) +
                     sizeof(RecordLength) +
                     sizeof(ECCOffset) +
                     sizeof(ECCLength));
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
    if (supportedRecords.end() != supportedRecords.find(name))
    {
        // If it's a record we're interested in, proceed to find
        // contained keywords and their values.
        auto kwMap = readKeywords(nameOffset + lengths::RECORD_NAME);
        // Add entry for this record (and contained keyword:value pairs)
        // to the parsed vpd output.
        _out.emplace(std::make_pair(name, kwMap));
    }
}

KeywordMap Impl::readKeywords(std::size_t kwOft)
{
    auto vpdBufferPtr = _vpd.data() +
                        kwOft;

    // Now we are the keyword entries for
    // a record
    KeywordMap map {};
    while (true)
    {
        // Note keyword name
        std::string kw(vpdBufferPtr, lengths::KW_NAME);
        if (LAST_KW == kw)
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
        if (supportedKeywords.end() != supportedKeywords.find(kw))
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

} // namespace parser
} // namespace vpd
} // namespace openpower
