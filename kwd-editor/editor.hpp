#pragma once

#include "types.hpp"

#include <cstddef>
#include <fstream>

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{

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
} // namespace

/** @class Editor
 *  @brief Implements VPD editing related functinality, currently
 *  implemented to support only keyword data update functionality.
 *
 *  An Editor object must be constructed by passing in VPD in
 *  binary format. To edit the keyword data, call the updateKeyword() method.
 *  The method looks for the record name to update in VTOC and
 *  then looks for the keyword name in that record.
 *  when found it updates the data of keyword with the given data.
 *  It does not block keyword data update in case the length of new data is
 *  greater than or less than the current data length.
 *  If the new data length is more than the length alotted to that keyword
 *  the new data will be truncated to update only the allotted length.
 *  Similarly if the new data length is less then only that much data will
 *  be updated for the keyword and remaining bits will be left unchanged.
 *
 *  Following is the algorithm used to update keyword:
 *  1) Look for the record name in the given VPD file
 *  2) Look for the keyword name for which data needs to be updated
 *     which is the table of contents record.
 *  3) update the data for that keyword with the new data
 */
class Editor
{
  public:
    Editor() = delete;
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    Editor(Editor&&) = delete;
    Editor& operator=(Editor&&) = delete;
    ~Editor() = default;

    /** @brief Construct an Editor
     *
     *  @param[in] path - Path to the vpd file
     */
    explicit Editor(inventory::Path path, std::string record, std::string kwd) :
        vpdFilePath(path)
    {
        thisRecord.recName = record;
        thisRecord.recKWd = kwd;
    }

    /** @brief Update the given keyword
     *
     *  @param[in] ptOffset - Offset to pt record
     *  @param[in] ptLength - length of PT record
     *  @param[in] kwdData - data to update
     *
     **/
    void updateKeyword(RecordOffset ptOffset, std::size_t ptLength,
                       Binary kwdData);

  private:
    /** @brief Checks if given record name exist in the VPD file
     *
     *  @param[in] ptOffset - offset to keyword data of PT keyword
     *    in VTOC record
     *  @param[in] ptLength - length of the PT record
     *
     **/
    void checkPTForRecord(RecordOffset ptOffset, std::size_t ptLength);

    /** @brief Checks for given keyword in the record
     **/
    void checkRecordForKwd();

    /** @brief update data for given keyword
     *
     *  @param[in] kwdData- data to be updated
     *
     **/
    void updateData(Binary kwdData);

    /** @brief update record ECC
     **/
    void updateRecordECC();

    // path to the VPD file to edit
    inventory::Path vpdFilePath;

    // stream to perform operation on file
    std::fstream vpdFileStream;

    // structure to hold info about record to edit
    struct RecInfo
    {
        Binary recData;
        Binary recEccData;
        std::string recName;
        std::string recKWd;
        RecordOffset recOffset;
        ECCOffset recECCoffset;
        // size_t is used here as ecc code needs size_t pointer and reinterpret
        // cast yields garbage value
        std::size_t recECCLength;
        std::size_t KwdDataLength;
        RecordSize recSize;
    } thisRecord;
}; // class editor

} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
