#pragma once

#include "const.hpp"
#include "types.hpp"

#include <cstddef>
#include <fstream>
#include <nlohmann/json.hpp>

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{
using namespace openpower::vpd::constants;

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
    explicit Editor(inventory::Path path, nlohmann::json json,
                    std::string record, std::string kwd) :
        vpdFilePath(path),
        jsonFile(json)
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

    /** @brief method to update cache once the data for kwd has been updated
     **/
    void updateCache();

    /** @brief method to process and update CI in case required
     *
     *  @param[in] - objectPath - path of the object to introspect
     *
     **/
    void processAndUpdateCI(const std::string objectPath);

    /** @brief method to process and update Extra Interface
     *  @param[in] Inventory - single inventory json subpart
     *  @param[in] objectPath - path of the object to introspect
     */
    void processAndUpdateEI(nlohmann::json Inventory, inventory::Path objPath);

    /** @brief method to make busctl call
     *
     *  @param[in] object - bus object path
     *  @param[in] interface - bus interface
     *  @param[in] property - property to update on BUS
     *  @param[in] data - data to be updayed on Bus
     *
     */
    template <typename T>
    void busctlCall(const std::string object, const std::string interface,
                    const std::string property, const std::variant<T> data);

    // path to the VPD file to edit
    inventory::Path vpdFilePath;

    // stream to perform operation on file
    std::fstream vpdFileStream;

    // file to store parsed json
    nlohmann::json jsonFile;

    // structure to hold info about record to edit
    struct RecInfo
    {
        Binary recData;
        Binary recEccData;
        Binary kwdupdatedData; // need access to it in case encoding is needed
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
