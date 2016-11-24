#pragma once

#include <cstddef>
#include "store.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{
namespace keyword
{

/** @brief Encoding scheme of a VPD keyword's data */
enum class Encoding
{
    ASCII, /**< data encoded in ascii */
    RAW,   /**< raw data */
    // Keywords needing custom decoding
    B1     /**< The keyword B1 needs to be decoded specially */
};

} // namespace keyword

namespace
{

using KeywordInfo = std::tuple<record::Keyword, keyword::Encoding>;
using OffsetList = std::vector<uint32_t>;
using KeywordMap = Parsed::mapped_type;

}

/** @class Impl
 *  @brief Implements parser for OpenPOWER VPD
 *
 *  An Impl object must be constructed by passing in OpenPOWER VPD in
 *  binary format. To parse the VPD, call the run() method. The run()
 *  method returns an openpower::vpd::Store object, which contains
 *  parsed VPD, and provides access methods for the VPD.
 *
 *  Following is the algorithm used to parse OpenPOWER VPD:
 *  1) Validate that the first record is VHDR, the header record.
 *  2) From the VHDR record, get the offset of the VTOC record,
 *     which is the table of contents record.
 *  3) Process the VTOC record - note offsets of supported records.
 *  4) For each supported record :
 *  4.1) Jump to record via offset. Add record name to parser output.
 *  4.2) Process record - for each contained and supported keyword:
 *  4.2.1) Note keyword name and value, associate this information to
 *         to the record noted in step 4.1).
 */
class Impl
{
    public:
        Impl() = delete;
        Impl(const Impl&) = delete;
        Impl& operator=(const Impl&) = delete;
        Impl(Impl&&) = delete;
        Impl& operator=(Impl&&) = delete;
        ~Impl() = default;

        /** @brief Construct an Impl
         *
         *  @param[in] vpdBuffer - Binary OpenPOWER VPD
         */
        explicit Impl(Binary&& vpdBuffer)
            : vpd(std::move(vpdBuffer)),
              out{}
        {}

        /** @brief Run the parser on binary OpenPOWER VPD
         *
         *  @returns openpower::vpd::Store object
         */
        Store run();

    private:
        /** @brief Process the table of contents record, VHDR
         *
         *  @returns List of offsets to records in VPD
         */
        OffsetList readTOC() const;

        /** @brief Read the PT keyword contained in the VHDR record,
         *         to obtain offsets to other records in the VPD.
         *
         *  @param[in] iterator - iterator to buffer containing VPD
         *  @param[in] ptLength - Length of PT keyword data
         *
         *  @returns List of offsets to records in VPD
         */
        OffsetList readPT(Binary::const_iterator iterator,
                          std::size_t ptLen) const;

        /** @brief Read keyword data
         *
         *  @param[in] keyword - OpenPOWER VPD keyword
         *  @param[in] dataLength - Length of data to be read
         *  @param[in] iterator - iterator pointing to a Keyword's data in
         *      the VPD
         *
         *  @returns keyword data as a string
         */
        std::string readKwData(const KeywordInfo& keyword,
                               std::size_t dataLength,
                               Binary::const_iterator iterator);

        /** @brief While we're pointing at the keyword section of
         *     a record in the VPD, this will read all contained
         *     keywords and their values.
         *
         *  @param[in] iterator - iterator pointing to a Keyword in the VPD
         *
         *  @returns map of keyword:data
         */
        KeywordMap readKeywords(Binary::const_iterator iterator);

        /** @brief Checks if the VHDR record is present in the VPD */
        void checkHeader() const;

        /** @brief OpenPOWER VPD in binary format */
        Binary vpd;

        /** @brief parser output */
        Parsed out;
};

} // namespace parser
} // namespace vpd
} // namespace openpower
