#include <sstream>
#include <exception>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <iomanip>
#include <tuple>
#include <algorithm>
#include "defines.hpp"
#include "impl.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{

static constexpr auto MAC_ADDRESS_LEN_BYTES = 6;
static constexpr auto LAST_KW = "PF";

static const std::unordered_map<std::string,
       KeywordInfo> supportedKeywords =
{
    {"DR", std::make_tuple(record::Keyword::DR, keyword::Encoding::ASCII)},
    {"PN", std::make_tuple(record::Keyword::PN, keyword::Encoding::ASCII)},
    {"SN", std::make_tuple(record::Keyword::SN, keyword::Encoding::ASCII)},
    {"CC", std::make_tuple(record::Keyword::CC, keyword::Encoding::ASCII)},
    {"HW", std::make_tuple(record::Keyword::HW, keyword::Encoding::RAW)},
    {"B1", std::make_tuple(record::Keyword::B1, keyword::Encoding::B1)},
    {"VN", std::make_tuple(record::Keyword::VN, keyword::Encoding::ASCII)},
    {"MB", std::make_tuple(record::Keyword::MB, keyword::Encoding::RAW)},
    {"MM", std::make_tuple(record::Keyword::MM, keyword::Encoding::ASCII)}
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

constexpr auto toHex(size_t c)
{
    constexpr auto map = "0123456789abcdef";
    return map[c];
}

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
        auto stop = std::next(iterator, lengths::RECORD_NAME);
        std::string record(iterator, stop);
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
    RecordOffset vtocOffset = vpd.at(offsets::VTOC_PTR);
    RecordOffset highByte = vpd.at(offsets::VTOC_PTR + 1);
    vtocOffset |= (highByte << 8);

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

    auto stop = std::next(iterator, lengths::RECORD_NAME);
    std::string record(iterator, stop);
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
        RecordOffset offset = *iterator;
        RecordOffset highByte = *(iterator + 1);
        offset |= (highByte << 8);
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

std::string Impl::readKwData(const KeywordInfo& keyword,
                             std::size_t dataLength,
                             Binary::const_iterator iterator)
{
    switch (std::get<keyword::Encoding>(keyword))
    {
        case keyword::Encoding::ASCII:
        {
            auto stop = std::next(iterator, dataLength);
            return std::string(iterator, stop);
        }

        case keyword::Encoding::RAW:
        {
            auto stop = std::next(iterator, dataLength);
            std::string data(iterator, stop);
            std::string result {};
            std::for_each(data.cbegin(), data.cend(),
                          [&result](size_t c)
                          {
                              result += toHex(c >> 4);
                              result += toHex(c & 0x0F);
                          });
            return result;
        }

        case keyword::Encoding::B1:
        {
            //B1 is MAC address, represent as AA:BB:CC:DD:EE:FF
            auto stop = std::next(iterator, MAC_ADDRESS_LEN_BYTES);
            std::string data(iterator, stop);
            std::string result {};
            auto strItr = data.cbegin();
            size_t firstDigit = *strItr;
            result += toHex(firstDigit >> 4);
            result += toHex(firstDigit & 0x0F);
            std::advance(strItr, 1);
            std::for_each(strItr, data.cend(),
                          [&result](size_t c)
                          {
                              result += ":";
                              result += toHex(c >> 4);
                              result += toHex(c & 0x0F);
                          });
            return result;
        }

        default:
            break;
    }

    return {};
}

KeywordMap Impl::readKeywords(Binary::const_iterator iterator)
{
    KeywordMap map {};
    while (true)
    {
        // Note keyword name
        std::string kw(iterator, iterator + lengths::KW_NAME);
        if (LAST_KW == kw)
        {
            // We're done
            break;
        }
        // Jump past keyword name
        std::advance(iterator, lengths::KW_NAME);
        // Note keyword data length
        std::size_t length = *iterator;
        // Jump past keyword length
        std::advance(iterator, sizeof(KwSize));
        // Pointing to keyword data now
        if (supportedKeywords.end() != supportedKeywords.find(kw))
        {
            // Keyword is of interest to us
            std::string data = readKwData(
                                   (supportedKeywords.find(kw))->second,
                                   length,
                                   iterator);
            map.emplace(std::move(kw), std::move(data));
        }
        // Jump past keyword data length
        std::advance(iterator, length);
    }

    return map;
}

} // namespace parser
} // namespace vpd
} // namespace openpower
