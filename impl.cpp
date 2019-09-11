#include "impl.hpp"

#include "defines.hpp"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <tuple>
#include <unordered_map>

namespace openpower
{
namespace vpd
{
namespace parser
{

static const std::unordered_map<std::string, Record> supportedRecords = {
    {"VINI", Record::VINI}, {"OPFR", Record::OPFR}, {"OSYS", Record::OSYS}};

static constexpr auto MAC_ADDRESS_LEN_BYTES = 6;
static constexpr auto LAST_KW = "PF";
static constexpr auto POUND_KW = '#';
static constexpr auto UUID_LEN_BYTES = 16;
static constexpr auto UUID_TIME_LOW_END = 8;
static constexpr auto UUID_TIME_MID_END = 13;
static constexpr auto UUID_TIME_HIGH_END = 18;
static constexpr auto UUID_CLK_SEQ_END = 23;

static constexpr auto MB_RESULT_LEN = 19;
static constexpr auto MB_LEN_BYTES = 8;
static constexpr auto MB_YEAR_END = 4;
static constexpr auto MB_MONTH_END = 7;
static constexpr auto MB_DAY_END = 10;
static constexpr auto MB_HOUR_END = 13;
static constexpr auto MB_MIN_END = 16;

static const std::unordered_map<std::string, internal::KeywordInfo>
    supportedKeywords = {
        {"DR", std::make_tuple(record::Keyword::DR, keyword::Encoding::ASCII)},
        {"PN", std::make_tuple(record::Keyword::PN, keyword::Encoding::ASCII)},
        {"SN", std::make_tuple(record::Keyword::SN, keyword::Encoding::ASCII)},
        {"CC", std::make_tuple(record::Keyword::CC, keyword::Encoding::ASCII)},
        {"HW", std::make_tuple(record::Keyword::HW, keyword::Encoding::RAW)},
        {"B1", std::make_tuple(record::Keyword::B1, keyword::Encoding::B1)},
        {"VN", std::make_tuple(record::Keyword::VN, keyword::Encoding::ASCII)},
        {"MB", std::make_tuple(record::Keyword::MB, keyword::Encoding::MB)},
        {"MM", std::make_tuple(record::Keyword::MM, keyword::Encoding::ASCII)},
        {"UD", std::make_tuple(record::Keyword::UD, keyword::Encoding::UD)},
        {"VP", std::make_tuple(record::Keyword::VP, keyword::Encoding::ASCII)},
        {"VS", std::make_tuple(record::Keyword::VS, keyword::Encoding::ASCII)},
};

namespace
{

using RecordId = uint8_t;
using RecordOffset = uint16_t;
using RecordSize = uint16_t;
using RecordType = uint16_t;
using RecordLength = uint16_t;
using KwSize = uint8_t;
using PoundKwSize = uint16_t;
using ECCOffset = uint16_t;
using ECCLength = uint16_t;

constexpr auto toHex(size_t c)
{
    constexpr auto map = "0123456789abcdef";
    return map[c];
}

} // namespace

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

internal::OffsetList Impl::readTOC() const
{
    internal::OffsetList offsets{};

    // The offset to VTOC could be 1 or 2 bytes long
    RecordOffset vtocOffset = vpd.at(offsets::VTOC_PTR);
    RecordOffset highByte = vpd.at(offsets::VTOC_PTR + 1);
    vtocOffset |= (highByte << 8);

    // Got the offset to VTOC, skip past record header and keyword header
    // to get to the record name.
    auto iterator = vpd.cbegin();
    std::advance(iterator, vtocOffset + sizeof(RecordId) + sizeof(RecordSize) +
                               // Skip past the RT keyword, which contains
                               // the record name.
                               lengths::KW_NAME + sizeof(KwSize));

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

internal::OffsetList Impl::readPT(Binary::const_iterator iterator,
                                  std::size_t ptLength) const
{
    internal::OffsetList offsets{};

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
        std::advance(iterator, sizeof(RecordSize) + sizeof(RecordLength) +
                                   sizeof(ECCOffset) + sizeof(ECCLength));
    }

    return offsets;
}

void Impl::processRecord(std::size_t recordOffset)
{
    // Jump to record name
    auto nameOffset = recordOffset + sizeof(RecordId) + sizeof(RecordSize) +
                      // Skip past the RT keyword, which contains
                      // the record name.
                      lengths::KW_NAME + sizeof(KwSize);
    // Get record name
    auto iterator = vpd.cbegin();
    std::advance(iterator, nameOffset);

    std::string name(iterator, iterator + lengths::RECORD_NAME);
#ifndef IPZ_PARSER
    if (supportedRecords.end() != supportedRecords.find(name))
    {
#endif
        // If it's a record we're interested in, proceed to find
        // contained keywords and their values.
        std::advance(iterator, lengths::RECORD_NAME);

#ifdef IPZ_PARSER
        // Reverse back to RT Kw, in ipz vpd, to Read RT KW & value
        std::advance(iterator, -(lengths::KW_NAME + sizeof(KwSize) +
                                 lengths::RECORD_NAME));
#endif
        auto kwMap = readKeywords(iterator);
        // Add entry for this record (and contained keyword:value pairs)
        // to the parsed vpd output.
        out.emplace(std::move(name), std::move(kwMap));
#ifndef IPZ_PARSER
    }
#endif
}

std::string Impl::readKwData(const internal::KeywordInfo& keyword,
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
            std::string result{};
            std::for_each(data.cbegin(), data.cend(), [&result](size_t c) {
                result += toHex(c >> 4);
                result += toHex(c & 0x0F);
            });
            return result;
        }

        case keyword::Encoding::MB:
        {
            // MB is BuildDate, represent as
            // 1997-01-01-08:30:00
            // <year>-<month>-<day>-<hour>:<min>:<sec>
            auto stop = std::next(iterator, MB_LEN_BYTES);
            std::string data(iterator, stop);
            std::string result;
            result.reserve(MB_LEN_BYTES);
            auto strItr = data.cbegin();
            std::advance(strItr, 1);
            std::for_each(strItr, data.cend(), [&result](size_t c) {
                result += toHex(c >> 4);
                result += toHex(c & 0x0F);
            });

            result.insert(MB_YEAR_END, 1, '-');
            result.insert(MB_MONTH_END, 1, '-');
            result.insert(MB_DAY_END, 1, '-');
            result.insert(MB_HOUR_END, 1, ':');
            result.insert(MB_MIN_END, 1, ':');

            return result;
        }

        case keyword::Encoding::B1:
        {
            // B1 is MAC address, represent as AA:BB:CC:DD:EE:FF
            auto stop = std::next(iterator, MAC_ADDRESS_LEN_BYTES);
            std::string data(iterator, stop);
            std::string result{};
            auto strItr = data.cbegin();
            size_t firstDigit = *strItr;
            result += toHex(firstDigit >> 4);
            result += toHex(firstDigit & 0x0F);
            std::advance(strItr, 1);
            std::for_each(strItr, data.cend(), [&result](size_t c) {
                result += ":";
                result += toHex(c >> 4);
                result += toHex(c & 0x0F);
            });
            return result;
        }

        case keyword::Encoding::UD:
        {
            // UD, the UUID info, represented as
            // 123e4567-e89b-12d3-a456-426655440000
            //<time_low>-<time_mid>-<time hi and version>
            //-<clock_seq_hi_and_res clock_seq_low>-<48 bits node id>
            auto stop = std::next(iterator, UUID_LEN_BYTES);
            std::string data(iterator, stop);
            std::string result{};
            std::for_each(data.cbegin(), data.cend(), [&result](size_t c) {
                result += toHex(c >> 4);
                result += toHex(c & 0x0F);
            });
            result.insert(UUID_TIME_LOW_END, 1, '-');
            result.insert(UUID_TIME_MID_END, 1, '-');
            result.insert(UUID_TIME_HIGH_END, 1, '-');
            result.insert(UUID_CLK_SEQ_END, 1, '-');

            return result;
        }
        default:
            break;
    }

    return {};
}

internal::KeywordMap Impl::readKeywords(Binary::const_iterator iterator)
{
    internal::KeywordMap map{};
    while (true)
    {
        // Note keyword name
        std::string kw(iterator, iterator + lengths::KW_NAME);
        if (LAST_KW == kw)
        {
            // We're done
            break;
        }
        // Check if the Keyword is '#kw'
        char kwNameStart = *iterator;

        // Jump past keyword name
        std::advance(iterator, lengths::KW_NAME);

        std::size_t length;
        std::size_t lengthHighByte;
        if (POUND_KW == kwNameStart)
        {
            // Note keyword data length
            length = *iterator;
            lengthHighByte = *(iterator + 1);
            length |= (lengthHighByte << 8);

            // Jump past 2Byte keyword length
            std::advance(iterator, sizeof(PoundKwSize));
        }
        else
        {
            // Note keyword data length
            length = *iterator;

            // Jump past keyword length
            std::advance(iterator, sizeof(KwSize));
        }

        // Pointing to keyword data now
#ifndef IPZ_PARSER
        if (supportedKeywords.end() != supportedKeywords.find(kw))
        {
            // Keyword is of interest to us
            std::string data = readKwData((supportedKeywords.find(kw))->second,
                                          length, iterator);
            map.emplace(std::move(kw), std::move(data));
        }

#else
        // support all the Keywords
        auto stop = std::next(iterator, length);
        std::string kwdata(iterator, stop);
        map.emplace(std::move(kw), std::move(kwdata));

#endif
        // Jump past keyword data length
        std::advance(iterator, length);
    }

    return map;
}

Store Impl::run()
{
    // Check if the VHDR record is present
    checkHeader();

    // Read the table of contents record, to get offsets
    // to other records.
    auto offsets = readTOC();
    for (const auto& offset : offsets)
    {
        processRecord(offset);
    }

    // Return a Store object, which has interfaces to
    // access parsed VPD by record:keyword
    return Store(std::move(out));
}

} // namespace parser
} // namespace vpd
} // namespace openpower
