#pragma once

#include "const.hpp"
#include "types.hpp"

#include <cstddef>
#include <fstream>
#include <tuple>

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{

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
class EditorImpl
{
  public:
    EditorImpl() = delete;
    EditorImpl(const EditorImpl&) = delete;
    EditorImpl& operator=(const EditorImpl&) = delete;
    EditorImpl(EditorImpl&&) = delete;
    EditorImpl& operator=(EditorImpl&&) = delete;
    ~EditorImpl() = default;

    /** @brief Construct EditorImpl class
     *
     *  @param[in] path - Path to the vpd file
     */
    EditorImpl(const inventory::Path& path, const std::string& record,
               const std::string& kwd) :
        vpdFilePath(path),
        thisRecord(record, kwd)
    {
    }

    /** @brief Update the given keyword
     *  @param[in] kwdData - data to update
     */
    void updateKeyword(const Binary& kwdData);

  private:
    /** @brief read VTOC record from the vpd file
     */
    void readVTOC();

    /** @brief validate ecc data for the VTOC record
     *  @param[in] tocRecData - VTOC record data
     *  @param[in] tocECCData - VTOC ECC data
     *  @param[in] recLength - Lenght of VTOC record
     *  @param[in] eccLength - Length of ECC record
     */
    void checkECC(const Binary& tocRecData, const Binary& tocECCData,
                  openpower::vpd::constants::RecordLength recLength,
                  openpower::vpd::constants::ECCLength eccLength);

    /** @brief reads value at the given offset
     *  @param[in] offset - offset value
     *  @return[out] - value at that offset in bigendian
     */
    auto getValue(openpower::vpd::constants::offsets::Offsets offset);

    /** @brief Checks if given record name exist in the VPD file
     *  @param[in] iterator - pointing to start of PT kwd
     *  @param[in] ptLength - length of the PT kwd
     */
    void checkPTForRecord(Binary::const_iterator& iterator, Byte ptLength);

    /** @brief Checks for given keyword in the record
     */
    void checkRecordForKwd();

    /** @brief update data for given keyword
     *  @param[in] kwdData- data to be updated
     */
    void updateData(Binary kwdData);

    /** @brief update record ECC
     */
    void updateRecordECC();

    // path to the VPD file to edit
    const inventory::Path& vpdFilePath;

    // stream to perform operation on file
    std::fstream vpdFileStream;

    // structure to hold info about record to edit
    struct RecInfo
    {
        Binary recData;
        Binary recEccData;
        const std::string& recName;
        const std::string& recKWd;
        openpower::vpd::constants::RecordOffset recOffset;
        openpower::vpd::constants::ECCOffset recECCoffset;
        std::size_t recECCLength;
        std::size_t kwdDataLength;
        openpower::vpd::constants::RecordSize recSize;

        // constructor
        RecInfo(const std::string& rec, const std::string& kwd) :
            recName(rec), recKWd(kwd), recOffset(0), recECCoffset(0),
            recECCLength(0), kwdDataLength(0), recSize(0)
        {
        }
    } thisRecord;

}; // class EditorImpl

} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
