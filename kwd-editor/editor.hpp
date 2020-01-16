#pragma once

#include <cstddef>
#include "types.hpp"

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
} //namespace

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
         *  @param[in] vpdBuffer - Binary VPD
         */
        explicit Editor(Binary& vpdFile):vpd(vpdFile)
        {

        }

        /** @brief Update the given keyword
         *
         *  @param[in] iterator - iterator to the beginning of PT record
         *  @param[in] ptLength - length of PT record
         *  @param[in] recName - record name
         *  @param[in] kwdName - keyword name
         *  @param[in] kwdData - data to update
         */
        void updateKeyword(Binary::const_iterator& iterator,
                            std::size_t ptLength, 
                            std::string recName,
                            std::string kwdName,
                            Binary kwdData);

    
    private:
        /** @brief Checks if given record name exist in the VPD file 
         * 
         *  @param[in] iterator - iterator pointing to keyword data of PT keyword
         *    in VTOC record
         *  @param[in] ptLength - length of the PT record
         *  @param[in] recName - Record name to find
         * 
         *  @returns offset to the record
        */
        RecordOffset checkPTForRecord(Binary::const_iterator iterator, 
                                            std::size_t ptLength, 
                                            std::string recName);

        /** @brief Checks for given keyword in the record 
         * 
         *  @param[in] iterator - iterator pointing to start of record
         *  @param[in] kwdName - keyword name
         *  @param[in] kwdData - data to be updated
         * 
        */
        void checkRecordForKwd(RecordOffset offset, std::string kwdName, Binary kwdData);

        /** @brief update data for given keyword 
         * 
         *  @param[in] kwdData- data to be updated
         *  @param[in] dataLength - existing data length of the kwd
         *  @param[in] iterator - pointing to the start of kwd data offset
         * 
        */
        void updateData(Binary kwdData, std::size_t dataLength, Binary::iterator iterator);

        //holds the vpd file
        Binary vpd;
}; //class editor

} //namespace editor
} //namespace keyword
} //namespace vpd
} //namespace openpower