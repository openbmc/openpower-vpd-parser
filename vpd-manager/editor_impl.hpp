#pragma once

#include "const.hpp"
#include "types.hpp"

#include <cstddef>
#include <fstream>
#include <nlohmann/json.hpp>
#include <tuple>

namespace openpower
{
namespace vpd
{
namespace manager
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
    EditorImpl(const inventory::Path& path, const nlohmann::json& json,
               const std::string& record, const std::string& kwd) :
        vpdFilePath(path),
        jsonFile(json), thisRecord(record, kwd)
    {
    }

    /** @brief Update data for keyword
     *  @param[in] kwdData - data to update
     */
    void updateKeyword(const Binary& kwdData);

  private:
    /** @brief read VTOC record from the vpd file
     */
    void readVTOC();

    /** @brief validate ecc data for the VTOC record
     *  @param[in] iterator to VTOC record data
     *  @param[in] iterator to VTOC ECC data
     *  @param[in] Lenght of VTOC record
     *  @param[in] Length of ECC record
     */
    void checkECC(Binary::const_iterator& itrToRecData,
                  Binary::const_iterator& itrToECCData,
                  openpower::vpd::constants::RecordLength recLength,
                  openpower::vpd::constants::ECCLength eccLength);

    /** @brief reads value at the given offset
     *  @param[in] offset - offset value
     *  @return  value at that offset in bigendian
     */
    auto getValue(openpower::vpd::constants::offsets::Offsets offset);

    /** @brief Checks if required record name exist in the VPD file
     *  @param[in] iterator - pointing to start of PT kwd
     *  @param[in] ptLength - length of the PT kwd
     */
    void checkPTForRecord(Binary::const_iterator& iterator, Byte ptLength);

    /** @brief Checks for reuired keyword in the record
     */
    void checkRecordForKwd();

    /** @brief update data for given keyword
     *  @param[in] kwdData- data to be updated
     */
    void updateData(const Binary& kwdData);

    /** @brief update record ECC
     */
    void updateRecordECC();

    /** @brief method to update cache once the data for kwd has been updated
     */
    void updateCache();

    /** @brief method to process and update CI in case required
     *  @param[in] - objectPath - path of the object to introspect
     */
    void processAndUpdateCI(const std::string& objectPath);

    /** @brief method to process and update Extra Interface
     *  @param[in] Inventory - single inventory json subpart
     *  @param[in] objectPath - path of the object to introspect
     */
    void processAndUpdateEI(const nlohmann::json& Inventory,
                            const inventory::Path& objPath);

    // path to the VPD file to edit
    const inventory::Path& vpdFilePath;

    // stream to perform operation on file
    std::fstream vpdFileStream;

    // file to store parsed json
    const nlohmann::json& jsonFile;

    // structure to hold info about record to edit
    struct RecInfo
    {
        Binary kwdUpdatedData; // need access to it in case encoding is needed
        const std::string& recName;
        const std::string& recKWd;
        openpower::vpd::constants::RecordOffset recOffset;
        openpower::vpd::constants::ECCOffset recECCoffset;
        std::size_t recECCLength;
        std::size_t kwdDataLength;
        openpower::vpd::constants::RecordSize recSize;
        openpower::vpd::constants::DataOffset kwDataOffset;
        // constructor
        RecInfo(const std::string& rec, const std::string& kwd) :
            recName(rec), recKWd(kwd), recOffset(0), recECCoffset(0),
            recECCLength(0), kwdDataLength(0), recSize(0), kwDataOffset(0)
        {
        }
    } thisRecord;

    Binary vpdFile;

}; // class EditorImpl

} // namespace editor
} // namespace manager
} // namespace vpd
} // namespace openpower
