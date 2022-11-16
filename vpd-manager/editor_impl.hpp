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

/** @class EditorImpl */
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
     *  @param[in] record - Record Name
     *  @param[in] kwd - Keyword
     *  @param[in] vpd - Vpd Vector
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
     *  @param[in] json - Parsed inventory json
     *  @param[in] record - Record name
     *  @param[in] kwd - Keyword
     *  @param[in] inventoryPath - Inventory path of the vpd
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

    /**
     * @brief Update data for keyword
     * The method looks for the record name to update in VTOC and then
     * looks for the keyword name in that record. when found it updates the data
     * of keyword with the given data. It does not block keyword data update in
     * case the length of new data is greater than or less than the current data
     * length. If the new data length is more than the length alotted to that
     * keyword the new data will be truncated to update only the allotted
     * length. Similarly if the new data length is less then only that much data
     * will be updated for the keyword and remaining bits will be left
     * unchanged.
     *
     * Following is the algorithm used to update keyword:
     * 1) Look for the record name in the given VPD file
     * 2) Look for the keyword name for which data needs to be updated
     *    which is the table of contents record.
     * 3) update the data for that keyword with the new data
     *
     * @param[in] kwdData - data to update
     * @param[in] offset - offset at which the VPD starts
     * @param[in] updCache - Flag which tells whether to update Cache or not.
     */
    void updateKeyword(const Binary& kwdData, uint32_t offset,
                       const bool& updCache);

    /** @brief Expands location code on DBUS
     *  @param[in] locationCodeType - "fcs" or "mts"
     */
    void expandLocationCode(const std::string& locationCodeType);

  private:
    /** @brief read VTOC record from the vpd file
     */
    void readVTOC();

    /** @brief validate ecc data for the VTOC record
     *  @param[in] itrToRecData -iterator to the record data
     *  @param[in] itrToECCData - iterator to the ECC data
     *  @param[in] recLength - Length of the record
     *  @param[in] eccLength - Length of the record's ECC
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

    /** @brief Checks for required keyword in the record */
    void checkRecordForKwd();

    /** @brief update data for given keyword
     *  @param[in] kwdData- data to be updated
     */
    void updateData(const Binary& kwdData);

    /** @brief update record ECC */
    void updateRecordECC();

    /** @brief method to update cache once the data for keyword has been updated
     */
    void updateCache();

    /** @brief method to process and update CI in case required
     *  @param[in] - objectPath - path of the object to introspect
     */
    void processAndUpdateCI(const std::string& objectPath);

    /** @brief method to process and update extra interface
     *  @param[in] Inventory - single inventory json subpart
     *  @param[in] objPath - path of the object to introspect
     */
    void processAndUpdateEI(const nlohmann::json& Inventory,
                            const inventory::Path& objPath);

    /** @brief method to make busctl call
     *
     *  @param[in] object - bus object path
     *  @param[in] interface - bus interface
     *  @param[in] property - property to update on BUS
     *  @param[in] data - data to be updated on Bus
     *
     */
    template <typename T>
    void makeDbusCall(const std::string& object, const std::string& interface,
                      const std::string& property, const std::variant<T>& data);

    /** @brief Method to check the record's Data using ECC */
    void checkRecordData();

    // path to the VPD file to edit
    inventory::Path vpdFilePath;

    // inventory path of the vpd fru to update keyword
    inventory::Path objPath{};

    // stream to perform operation on file
    std::fstream vpdFileStream;

    // stream to operate on VPD data
    std::fstream vpdDataFileStream;

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
}; // class EditorImpl

} // namespace editor
} // namespace manager
} // namespace vpd
} // namespace openpower