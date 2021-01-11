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
    ~EditorImpl()
    {
    }

    /** @brief Construct EditorImpl class
     *
     *  @param[in] path - Path to the vpd file
     */
    EditorImpl(const std::string& record, const std::string& kwd,
               Binary&& vpd) :
        startOffset(0),
        thisRecord(record, kwd), vpdFile(std::move(vpd))
    {
    }

    /** @brief Construct EditorImpl class
     *
     *  @param[in] path - Path to the vpd file
     */
    EditorImpl(const inventory::Path& path, const nlohmann::json& json,
               const std::string& record, const std::string& kwd,
               const sdbusplus::message::object_path& inventoryPath) :
        vpdFilePath(path),
        objPath(inventoryPath), startOffset(0), jsonFile(json),
        thisRecord(record, kwd)
    {
    }

    /** @brief Construct EditorImpl class
     *
     * @param[in] path - EEPROM path
     * @param[in] json - Parsed inventory json object
     * @param[in] record - Record name
     * @param[in] kwd - Keyword name
     */
    EditorImpl(const inventory::Path& path, const nlohmann::json& json,
               const std::string& record, const std::string& kwd) :
        vpdFilePath(path),
        jsonFile(json), thisRecord(record, kwd)
    {
    }

    /** @brief Update data for keyword
     *  @param[in] kwdData - data to update
     *  @param[in] updCache - Flag which tells whether to update Cache or not.
     */
    void updateKeyword(const Binary& kwdData, const bool& updCache);

    /** @brief Expands location code on DBUS
     *  @param[in] locationCodeType - "fcs" or "mts"
     */
    void expandLocationCode(const std::string& locationCodeType);

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

    /** @brief method to make busctl call
     *
     *  @param[in] object - bus object path
     *  @param[in] interface - bus interface
     *  @param[in] property - property to update on BUS
     *  @param[in] data - data to be updayed on Bus
     *
     */
    template <typename T>
    void makeDbusCall(const std::string& object, const std::string& interface,
                      const std::string& property, const std::variant<T>& data);

    // path to the VPD file to edit
    inventory::Path vpdFilePath;

    // inventory path of fru/module to update keyword
    const inventory::Path objPath;

    // stream to perform operation on file
    std::fstream vpdFileStream;

    // offset to get vpd data from EEPROM
    uint32_t startOffset;

    // file to store parsed json
    const nlohmann::json jsonFile;

    // structure to hold info about record to edit
    struct RecInfo
    {
        Binary kwdUpdatedData; // need access to it in case encoding is needed
        const std::string recName;
        const std::string recKWd;
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

    // If requested Interface is common Interface
    bool isCI;

    /** @brief This API will be used to find out Parent FRU of Module/CPU
     *
     * @param[in] - moduleObjPath, object path of that FRU
     * @param[in] - fruType, Type of Parent FRU
     *              for Module/CPU Parent Type- FruAndModule
     *
     * @return returns vpd file path of Parent Fru of that Module
     */
    std::string getSysPathForThisFruType(const std::string& moduleObjPath,
                                         const std::string& fruType);

    /** @brief This API will search for correct EEPROM path for asked CPU
     *         and will init vpdFilePath
     */
    void getVpdPathForCpu();

}; // class EditorImpl

} // namespace editor
} // namespace manager
} // namespace vpd
} // namespace openpower
