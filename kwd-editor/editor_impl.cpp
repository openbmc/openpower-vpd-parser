#include "config.h"

#include "editor_impl.hpp"

#include "utils.hpp"

#include "vpdecc/vpdecc.h"

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{
using namespace openpower::vpd::constants;

void EditorImpl::checkPTForRecord(Binary::const_iterator& iterator,
                                  Byte ptLength)
{
    // auto iterator = ptRecord.cbegin();
    auto end = std::next(iterator, ptLength + 1);

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
                                       sizeof(RecordOffset) +
                                       sizeof(RecordLength) +
                                       sizeof(ECCOffset) + sizeof(ECCLength));
        }
    }

    // imples the record was not found
    throw std::runtime_error("Record not found");
}

void EditorImpl::updateData(Binary kwdData)
{
    std::size_t lengthToUpdate = kwdData.size() <= thisRecord.kwdDataLength
                                     ? kwdData.size()
                                     : thisRecord.kwdDataLength;

    auto iteratorToNewdata = kwdData.cbegin();
    auto end = iteratorToNewdata;
    std::advance(end, lengthToUpdate);

    std::size_t curPos = vpdFileStream.tellg();

    std::copy(iteratorToNewdata, end,
              std::ostreambuf_iterator<char>(vpdFileStream));

    // get a hold to new data in case encoding is needed
    vpdFileStream.seekg(curPos, std::ios::beg);
    thisRecord.kwdUpdatedData.resize(thisRecord.kwdDataLength);
    vpdFileStream.read(
        reinterpret_cast<char*>((thisRecord.kwdUpdatedData).data()),
        thisRecord.kwdDataLength);
}

void EditorImpl::checkRecordForKwd()
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

    std::size_t dataLength = 0;
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
            thisRecord.kwdDataLength = dataLength;
            return;
        }

        // jump the data of current kwd to point to next kwd name
        std::advance(iterator, dataLength);
    }

    throw std::runtime_error("Keyword not found");
}

void EditorImpl::updateRecordECC()
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

auto EditorImpl::getValue(offsets::Offsets offset)
{
    Byte data = 0;
    vpdFileStream.seekg(offset, std::ios::beg)
        .get(*(reinterpret_cast<char*>(&data)));
    LE2ByteData lowByte = data;

    vpdFileStream.seekg(offset + 1, std::ios::beg)
        .get(*(reinterpret_cast<char*>(&data)));
    LE2ByteData highByte = data;
    lowByte |= (highByte << 8);

    return lowByte;
}

void EditorImpl::checkECC(const Binary& tocRecData, const Binary& tocECCData,
                          RecordLength recLength, ECCLength eccLength)
{
    auto l_status =
        vpdecc_check_data(const_cast<uint8_t*>(&tocRecData[0]), recLength,
                          const_cast<uint8_t*>(&tocECCData[0]), eccLength);
    if (l_status != VPD_ECC_OK)
    {
        throw std::runtime_error("Ecc check failed for VTOC");
    }
}

void EditorImpl::readVTOC()
{
    // read VTOC offset
    RecordOffset tocOffset = getValue(offsets::VTOC_PTR);

    // read VTOC record length
    RecordLength tocLength = getValue(offsets::VTOC_REC_LEN);

    // read TOC ecc offset
    ECCOffset tocECCOffset = getValue(offsets::VTOC_ECC_OFF);

    // read TOC ecc length
    ECCLength tocECCLength = getValue(offsets::VTOC_ECC_LEN);

    // read toc record data
    Binary vtocRecord(tocLength);
    vpdFileStream.seekg(tocOffset, std::ios::beg);
    vpdFileStream.read(reinterpret_cast<char*>(vtocRecord.data()), tocLength);

    // read toc ECC for ecc check
    Binary vtocECC(tocECCLength);
    vpdFileStream.seekg(tocECCOffset, std::ios::beg);
    vpdFileStream.read(reinterpret_cast<char*>(vtocECC.data()), tocECCLength);

    auto iterator = vtocRecord.cbegin();

    // to get to the record name.
    std::advance(iterator, sizeof(RecordId) + sizeof(RecordSize) +
                               // Skip past the RT keyword, which contains
                               // the record name.
                               lengths::KW_NAME + sizeof(KwSize));

    std::string recordName(iterator, iterator + lengths::RECORD_NAME);

    if ("VTOC" != recordName)
    {
        throw std::runtime_error("VTOC record not found");
    }

    // validate ecc for the record
    checkECC(vtocRecord, vtocECC, tocLength, tocECCLength);

    // jump to length of PT kwd
    std::advance(iterator, lengths::RECORD_NAME + lengths::KW_NAME);

    // Note size of PT
    Byte ptLen = *iterator;
    std::advance(iterator, 1);

    checkPTForRecord(iterator, ptLen);
}

template <typename T>
void EditorImpl::makeDbusCall(const std::string& object,
                              const std::string& interface,
                              const std::string& property,
                              const std::variant<T>& data)
{
    auto bus = sdbusplus::bus::new_default();
    auto properties =
        bus.new_method_call(INVENTORY_MANAGER_SERVICE, object.c_str(),
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

void EditorImpl::processAndUpdateCI(const std::string& objectPath)
{
    for (auto& commonInterface : jsonFile["commonInterfaces"].items())
    {
        for (auto& ci_propertyList : commonInterface.value().items())
        {
            if (ci_propertyList.value().type() ==
                nlohmann::json::value_t::object)
            {
                if ((ci_propertyList.value().value("recordName", "") ==
                     thisRecord.recName) &&
                    (ci_propertyList.value().value("keywordName", "") ==
                     thisRecord.recKWd))
                {
                    std::string kwdData(thisRecord.kwdUpdatedData.begin(),
                                        thisRecord.kwdUpdatedData.end());

                    makeDbusCall<std::string>((INVENTORY_PATH + objectPath),
                                          commonInterface.key(),
                                          ci_propertyList.key(), kwdData);
                }
            }
        }
    }
}

void EditorImpl::processAndUpdateEI(const nlohmann::json& Inventory,
                                    const inventory::Path& objPath)
{
    for (const auto& extraInterface : Inventory["extraInterfaces"].items())
    {
        if (extraInterface.value() != NULL)
        {
            for (const auto& ei_PropertyList : extraInterface.value().items())
            {
                if (ei_PropertyList.value().type() ==
                    nlohmann::json::value_t::object)
                {
                    if ((ei_PropertyList.value().value("recordName", "") ==
                         thisRecord.recName) &&
                        ((ei_PropertyList.value().value("keywordName", "") ==
                          thisRecord.recKWd)))
                    {
                    std::string kwdData(thisRecord.kwdUpdatedData.begin(),
                                        thisRecord.kwdUpdatedData.end());
                    makeDbusCall<std::string>(
                        (INVENTORY_PATH + objPath), extraInterface.key(),
                        ei_PropertyList.key(),
                        encodeKeyword(kwdData, ei_PropertyList.value().value(
                                                   "encoding", "")));
                    }
                }
            }
        }
    }
}

void EditorImpl::updateCache()
{
    const std::vector<nlohmann::json>& groupEEPROM =
        jsonFile["frus"][vpdFilePath].get_ref<const nlohmann::json::array_t&>();

    // iterate through all the inventories for this file path
    for (const auto& singleInventory : groupEEPROM)
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
            makeDbusCall<Binary>(
                (INVENTORY_PATH +
                 singleInventory["inventoryPath"].get<std::string>()),
                (IPZ_INTERFACE + (std::string) "." + thisRecord.recName),
                thisRecord.recKWd, thisRecord.kwdUpdatedData);

            // process Common interface
            processAndUpdateCI(singleInventory["inventoryPath"]
                                   .get_ref<const nlohmann::json::string_t&>());
        }

        // process extra interfaces
        processAndUpdateEI(singleInventory,
                           singleInventory["inventoryPath"]
                               .get_ref<const nlohmann::json::string_t&>());
    }
}

void EditorImpl::updateKeyword(const Binary& kwdData)
{
    vpdFileStream.open(vpdFilePath,
                       std::ios::in | std::ios::out | std::ios::binary);
    if (!vpdFileStream)
    {
        throw std::runtime_error("unable to open vpd file to edit");
    }

    // process VTOC for PTT rkwd
    readVTOC();

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
