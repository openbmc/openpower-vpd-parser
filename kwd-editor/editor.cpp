#include "editor.hpp"

#include "utils.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

#include "vpdecc/vpdecc.h"

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{
void Editor::checkPTForRecord(RecordOffset ptOffset, std::size_t ptLength)
{
    vpdFileStream.seekg(ptOffset);
    Binary PTrecord(ptLength);

    vpdFileStream.read(reinterpret_cast<char*>(PTrecord.data()), ptLength);

    auto iterator = PTrecord.cbegin();
    auto end = PTrecord.cend();

    // Look at each entry in the PT keyword for the record name
    while (iterator < end)
    {
        auto stop = std::next(iterator, lengths::RECORD_NAME);
        std::string record(iterator, stop);

        if (record == thisRecord.recName)
        {
            // Skip record name and record type
            std::advance(iterator, lengths::RECORD_NAME + sizeof(RecordType));

            // Get record offset
            thisRecord.recOffset = readUInt16LE(iterator);

            // pass the record offset length to read record length
            std::advance(iterator, lengths::RECORD_OFFSET);
            thisRecord.recSize = readUInt16LE(iterator);

            std::advance(iterator, lengths::RECORD_LENGTH);
            thisRecord.recECCoffset = readUInt16LE(iterator);

            ECCLength len;
            std::advance(iterator, lengths::RECORD_ECC_OFFSET);
            len = readUInt16LE(iterator);
            thisRecord.recECCLength = len;

            // once we find the record we don't need to look further
            return;
        }
        else
        {
            // Jump the record
            std::advance(iterator, lengths::RECORD_NAME + sizeof(RecordType) +
                                       sizeof(RecordSize) +
                                       sizeof(RecordLength) +
                                       sizeof(ECCOffset) + sizeof(ECCLength));
        }
    }

    // imples the record was not found
    throw std::runtime_error("Record not found");
}

void Editor::updateData(Binary kwdData)
{
    std::size_t lengthToUpdate = kwdData.size() <= thisRecord.KwdDataLength
                                     ? kwdData.size()
                                     : thisRecord.KwdDataLength;

    auto iteratorToNewdata = kwdData.cbegin();
    auto end = iteratorToNewdata;
    std::advance(end, lengthToUpdate);

    std::size_t curPos = vpdFileStream.tellg();

    std::copy(iteratorToNewdata, end,
              std::ostreambuf_iterator<char>(vpdFileStream));

    // get a hold to new data in case encoding is needed
    vpdFileStream.seekg(curPos, std::ios::beg);
    thisRecord.kwdupdatedData.resize(thisRecord.KwdDataLength);
    vpdFileStream.read(
        reinterpret_cast<char*>((thisRecord.kwdupdatedData).data()),
        thisRecord.KwdDataLength);
}

void Editor::checkRecordForKwd()
{
    RecordOffset recOffset = thisRecord.recOffset;

    // Jump to record name
    auto nameOffset = recOffset + sizeof(RecordId) + sizeof(RecordSize) +
                      // Skip past the RT keyword, which contains
                      // the record name.
                      lengths::KW_NAME + sizeof(KwSize);

    vpdFileStream.seekg(nameOffset + lengths::RECORD_NAME, std::ios::beg);

    (thisRecord.recData).resize(thisRecord.recSize);
    vpdFileStream.read(reinterpret_cast<char*>((thisRecord.recData).data()),
                       thisRecord.recSize);

    auto iterator = (thisRecord.recData).cbegin();
    auto end = (thisRecord.recData).cend();

    std::size_t dataLength;
    while (iterator < end)
    {
        // Note keyword name
        std::string kw(iterator, iterator + lengths::KW_NAME);

        // Check if the Keyword starts with '#'
        char kwNameStart = *iterator;
        std::advance(iterator, lengths::KW_NAME);

        // if keyword starts with #
        if (POUND_KW == kwNameStart)
        {
            // Note existing keyword data length
            dataLength = readUInt16LE(iterator);

            // Jump past 2Byte keyword length + data
            std::advance(iterator, sizeof(PoundKwSize));
        }
        else
        {
            // Note existing keyword data length
            dataLength = *iterator;

            // Jump past keyword length and data
            std::advance(iterator, sizeof(KwSize));
        }

        if (thisRecord.recKWd == kw)
        {
            // We're done
            std::size_t kwdOffset =
                std::distance((thisRecord.recData).cbegin(), iterator);
            vpdFileStream.seekp(nameOffset + lengths::RECORD_NAME + kwdOffset,
                                std::ios::beg);
            thisRecord.KwdDataLength = dataLength;
            return;
        }

        // jump the data of current kwd to point to next kwd name
        std::advance(iterator, dataLength);
    }

    throw std::runtime_error("Keyword not found");
}

void Editor::updateRecordECC()
{
    vpdFileStream.seekp(thisRecord.recECCoffset, std::ios::beg);

    (thisRecord.recEccData).resize(thisRecord.recECCLength);
    vpdFileStream.read(reinterpret_cast<char*>((thisRecord.recEccData).data()),
                       thisRecord.recECCLength);

    auto recPtr = (thisRecord.recData).cbegin();
    auto recEccPtr = (thisRecord.recEccData).cbegin();

    auto l_status = vpdecc_create_ecc(
        const_cast<uint8_t*>(&recPtr[0]), thisRecord.recSize,
        const_cast<uint8_t*>(&recEccPtr[0]), &thisRecord.recECCLength);
    if (l_status != VPD_ECC_OK)
    {
        throw std::runtime_error("Ecc update failed");
    }

    auto end = (thisRecord.recEccData).cbegin();
    std::advance(end, thisRecord.recECCLength);

    std::copy((thisRecord.recEccData).cbegin(), end,
              std::ostreambuf_iterator<char>(vpdFileStream));
}

template <typename T>
void Editor::busctlCall(const std::string object, const std::string interface,
                        const std::string property, const std::variant<T> data)
{
    auto bus = sdbusplus::bus::new_default();
    auto properties =
        bus.new_method_call(service.c_str(), object.c_str(),
                            "org.freedesktop.DBus.Properties", "Set");
    properties.append(interface);
    properties.append(property);
    properties.append(data);

    auto result = bus.call(properties);

    if (result.is_method_error())
    {
        throw std::runtime_error("bus call failed");
    }
}

void Editor::processAndUpdateCI(const std::string objectPath)
{
    for (auto& commonInterface : jsonFile["commonInterfaces"].items())
    {
        for (auto& ci_propertyList : commonInterface.value().items())
        {
            if ((ci_propertyList.value().value("recordName", "") ==
                 thisRecord.recName) &&
                (ci_propertyList.value().value("keywordName", "") ==
                 thisRecord.recKWd))
            {
                std::string kwdData(thisRecord.kwdupdatedData.begin(),
                                    thisRecord.kwdupdatedData.end());

                busctlCall<std::string>((VPD_OBJ_PATH_PREFIX + objectPath),
                                        commonInterface.key(),
                                        ci_propertyList.key(), kwdData);
            }
        }
    }
}

void Editor::processAndUpdateEI(nlohmann::json Inventory,
                                inventory::Path objPath)
{
    for (auto& extraInterface : Inventory["extraInterfaces"].items())
    {
        if (extraInterface.value() != NULL)
        {
            for (auto& ei_propertyList : extraInterface.value().items())
            {
                if ((ei_propertyList.value().value("recordName", "") ==
                     thisRecord.recName) &&
                    ((ei_propertyList.value().value("keywordName", "") ==
                      thisRecord.recKWd)))
                {
                    std::string kwdData(thisRecord.kwdupdatedData.begin(),
                                        thisRecord.kwdupdatedData.end());
                    busctlCall<std::string>(
                        (VPD_OBJ_PATH_PREFIX + objPath), extraInterface.key(),
                        ei_propertyList.key(),
                        encodeKeyword(kwdData, ei_propertyList.value().value(
                                                   "encoding", "")));
                }
            }
        }
    }
}
void Editor::updateCache()
{
    std::vector<nlohmann::json> groupEEPROM =
        jsonFile["frus"][vpdFilePath].get<std::vector<nlohmann::json>>();

    // iterate through all the inventories for this file path
    for (auto& singleInventory : groupEEPROM)
    {
        // by default inherit property is true
        bool isInherit = true;

        if (singleInventory.find("inherit") != singleInventory.end())
        {
            isInherit = singleInventory["inherit"].get<bool>();
        }

        if (isInherit)
        {
            // update com interface
            busctlCall<Binary>(
                (VPD_OBJ_PATH_PREFIX +
                 singleInventory["inventoryPath"].get<std::string>()),
                (COM_INTERFACE_PREFIX + "." + thisRecord.recName),
                thisRecord.recKWd, thisRecord.kwdupdatedData);

            // process Common interface
            processAndUpdateCI(
                singleInventory["inventoryPath"].get<std::string>());
        }

        // process extra interfaces
        processAndUpdateEI(singleInventory,
                           singleInventory["inventoryPath"].get<std::string>());
    }
}

void Editor::updateKeyword(RecordOffset ptOffset, std::size_t ptLength,
                           Binary kwdData)
{
    vpdFileStream.open(vpdFilePath,
                       std::ios::in | std::ios::out | std::ios::binary);
    if (!vpdFileStream)
    {
        throw std::runtime_error("unable to open vpd file to edit");
    }

    // search PT for record name
    checkPTForRecord(ptOffset, ptLength);

    // check record for keywrod
    checkRecordForKwd();

    // update the data to the file
    updateData(kwdData);

    // update the ECC data for the record once data has been updated
    updateRecordECC();

    // update the cache once data has been updated
    updateCache();
}

} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
