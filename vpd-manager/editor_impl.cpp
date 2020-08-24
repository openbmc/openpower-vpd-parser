#include "config.h"

#include "editor_impl.hpp"

#include "ipz_parser.hpp"
#include "parser_factory.hpp"
#include "utils.hpp"

#include "vpdecc/vpdecc.h"

using namespace openpower::vpd::parser::interface;
using namespace openpower::vpd::constants;
using namespace openpower::vpd::parser::factory;
using namespace openpower::vpd::ipz::parser;

namespace openpower
{
namespace vpd
{
namespace manager
{
namespace editor
{

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
    std::string errorMsg =
        string("Record \"") + thisRecord.recName +
        ("\" not found in VTOC's PT keyword in this vpd : \"") + vpdFilePath +
        "\".";
    throw std::runtime_error(errorMsg);
}

void EditorImpl::updateData(const Binary& kwdData)
{
    std::size_t lengthToUpdate = kwdData.size() <= thisRecord.kwdDataLength
                                     ? kwdData.size()
                                     : thisRecord.kwdDataLength;

    auto iteratorToNewdata = kwdData.cbegin();
    auto end = iteratorToNewdata;
    std::advance(end, lengthToUpdate);

    // update data in file buffer as it will be needed to update ECC
    // avoiding extra stream operation here
    auto iteratorToKWdData = vpdFile.begin();
    std::advance(iteratorToKWdData, thisRecord.kwDataOffset);
    std::copy(iteratorToNewdata, end, iteratorToKWdData);

#ifdef ManagerTest
    auto startItr = vpdFile.begin();
    std::advance(iteratorToKWdData, thisRecord.kwDataOffset);
    auto endItr = startItr;
    std::advance(endItr, thisRecord.kwdDataLength);

    Binary updatedData(startItr, endItr);
    if (updatedData == kwdData)
    {
        std::string errorMsg = string("The given data ");
        // errorMsg.append(updatedData);
        errorMsg += ", is updated in the given record " + thisRecord.recName +
                    (", in the given keyword ") + thisRecord.recKWd +
                    (", in the vpd \"") + vpdFilePath + ("\".");
        throw std::runtime_error(errorMsg);
    }
#else

    // update data in EEPROM as well. As we will not write complete file back
    vpdFileStream.seekg(offset + thisRecord.kwDataOffset, std::ios::beg);

    iteratorToNewdata = kwdData.cbegin();
    std::copy(iteratorToNewdata, end,
              std::ostreambuf_iterator<char>(vpdFileStream));

    // get a hold to new data in case encoding is needed
    thisRecord.kwdUpdatedData.resize(thisRecord.kwdDataLength);
    auto itrToKWdData = vpdFile.cbegin();
    std::advance(itrToKWdData, thisRecord.kwDataOffset);
    auto kwdDataEnd = itrToKWdData;
    std::advance(kwdDataEnd, thisRecord.kwdDataLength);
    std::copy(itrToKWdData, kwdDataEnd, thisRecord.kwdUpdatedData.begin());
#endif
}

void EditorImpl::checkRecordForKwd()
{
    RecordOffset recOffset = thisRecord.recOffset;

    // Amount to skip for record ID, size, and the RT keyword
    constexpr auto skipBeg = sizeof(RecordId) + sizeof(RecordSize) +
                             lengths::KW_NAME + sizeof(KwSize);

    auto iterator = vpdFile.cbegin();
    std::advance(iterator, recOffset + skipBeg + lengths::RECORD_NAME);

    auto end = iterator;
    std::advance(end, thisRecord.recSize);
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
            thisRecord.kwDataOffset = std::distance(vpdFile.cbegin(), iterator);
            thisRecord.kwdDataLength = dataLength;
            return;
        }

        // jump the data of current kwd to point to next kwd name
        std::advance(iterator, dataLength);
    }

    std::string errorMsg =
        std::string("The given Keyword ") + thisRecord.recKWd +
        (", is not found in the given record ") + thisRecord.recName +
        (", in the vpd \"") + vpdFilePath + ("\".");
    throw std::runtime_error(errorMsg);
}

void EditorImpl::updateRecordECC()
{
    auto itrToRecordData = vpdFile.cbegin();
    std::advance(itrToRecordData, thisRecord.recOffset);

    auto itrToRecordECC = vpdFile.cbegin();
    std::advance(itrToRecordECC, thisRecord.recECCoffset);

    auto l_status = vpdecc_create_ecc(
        const_cast<uint8_t*>(&itrToRecordData[0]), thisRecord.recSize,
        const_cast<uint8_t*>(&itrToRecordECC[0]), &thisRecord.recECCLength);
    if (l_status != VPD_ECC_OK)
    {
        std::string errorMsg = std::string("ECC update for the given record ") +
                               thisRecord.recName + (" failed for the vpd \"") +
                               vpdFilePath + ("\".");
        throw std::runtime_error(errorMsg);
    }

    auto end = itrToRecordECC;
    std::advance(end, thisRecord.recECCLength);

#ifndef ManagerTest
    vpdFileStream.seekp(offset + thisRecord.recECCoffset, std::ios::beg);
    std::copy(itrToRecordECC, end,
              std::ostreambuf_iterator<char>(vpdFileStream));
#endif
}

auto EditorImpl::getValue(offsets::Offsets offset)
{
    auto itr = vpdFile.cbegin();
    std::advance(itr, offset);
    LE2ByteData lowByte = *itr;
    LE2ByteData highByte = *(itr + 1);
    lowByte |= (highByte << 8);

    return lowByte;
}

void EditorImpl::checkECC(Binary::const_iterator& itrToRecData,
                          Binary::const_iterator& itrToECCData,
                          RecordLength recLength, ECCLength eccLength)
{
    auto l_status =
        vpdecc_check_data(const_cast<uint8_t*>(&itrToRecData[0]), recLength,
                          const_cast<uint8_t*>(&itrToECCData[0]), eccLength);

    if (l_status != VPD_ECC_OK)
    {
        std::string errorMsg =
            std::string("ECC check failed for VTOC record in the vpd \"") +
            vpdFilePath + ("\".");
        throw std::runtime_error(errorMsg);
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

    auto itrToRecord = vpdFile.cbegin();
    std::advance(itrToRecord, tocOffset);

    auto iteratorToECC = vpdFile.cbegin();
    std::advance(iteratorToECC, tocECCOffset);

    // validate ecc for the record
    checkECC(itrToRecord, iteratorToECC, tocLength, tocECCLength);

    // to get to the record name.
    std::advance(itrToRecord, sizeof(RecordId) + sizeof(RecordSize) +
                                  // Skip past the RT keyword, which contains
                                  // the record name.
                                  lengths::KW_NAME + sizeof(KwSize));

    std::string recordName(itrToRecord, itrToRecord + lengths::RECORD_NAME);

    if ("VTOC" != recordName)
    {
        std::string errorMsg =
            std::string("VTOC record not found in the vpd \"") + vpdFilePath +
            ("\".");
        throw std::runtime_error(errorMsg);
    }

    // jump to length of PT kwd
    std::advance(itrToRecord, lengths::RECORD_NAME + lengths::KW_NAME);

    // Note size of PT
    Byte ptLen = *itrToRecord;
    std::advance(itrToRecord, 1);

    checkPTForRecord(itrToRecord, ptLen);
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
        std::string errorMsg =
            string("Dbus method call failed while trying to Set the data: ");
        // errorMsg.append(std::string(data));
        errorMsg += " to the property: ";
        errorMsg.append(property);
        errorMsg += " in the interface: ";
        errorMsg.append(interface);
        errorMsg += " for the object: ";
        errorMsg.append(object);
        errorMsg += " in the service: ";
        errorMsg.append(INVENTORY_MANAGER_SERVICE);
        errorMsg += (".");

        throw std::runtime_error(errorMsg);
    }
}

void EditorImpl::processAndUpdateCI(const std::string& objectPath)
{
    for (auto& commonInterface : jsonFile["commonInterfaces"].items())
    {
        for (auto& ciPropertyList : commonInterface.value().items())
        {
            if (ciPropertyList.value().type() ==
                nlohmann::json::value_t::object)
            {
                if ((ciPropertyList.value().value("recordName", "") ==
                     thisRecord.recName) &&
                    (ciPropertyList.value().value("keywordName", "") ==
                     thisRecord.recKWd))
                {
                    std::string kwdData(thisRecord.kwdUpdatedData.begin(),
                                        thisRecord.kwdUpdatedData.end());

                    makeDbusCall<std::string>((INVENTORY_PATH + objectPath),
                                              commonInterface.key(),
                                              ciPropertyList.key(), kwdData);
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
            for (const auto& eiPropertyList : extraInterface.value().items())
            {
                if (eiPropertyList.value().type() ==
                    nlohmann::json::value_t::object)
                {
                    if ((eiPropertyList.value().value("recordName", "") ==
                         thisRecord.recName) &&
                        ((eiPropertyList.value().value("keywordName", "") ==
                          thisRecord.recKWd)))
                    {
                        std::string kwdData(thisRecord.kwdUpdatedData.begin(),
                                            thisRecord.kwdUpdatedData.end());
                        makeDbusCall<std::string>(
                            (INVENTORY_PATH + objPath), extraInterface.key(),
                            eiPropertyList.key(),
                            encodeKeyword(kwdData, eiPropertyList.value().value(
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
        bool isInheritEI = true;
        bool isCpuModuleOnly = false;

        if (singleInventory.find("inherit") != singleInventory.end())
        {
            isInherit = singleInventory["inherit"].get<bool>();
        }

        if (singleInventory.find("inheritEI") != singleInventory.end())
        {
            isInheritEI = singleInventory["inheritEI"].get<bool>();
        }

        // "type" exists only in CPU module and FRU
        if (singleInventory.find("type") != singleInventory.end())
        {
            if (singleInventory["type"] == "moduleOnly")
            {
                isCpuModuleOnly = true;
            }
        }

        if (isInherit)
        {
            // update com interface
            // For CPU- update  com interface only when isCI true
            if ((!isCpuModuleOnly) || (isCpuModuleOnly && isCI))
            {
                makeDbusCall<Binary>(
                    (INVENTORY_PATH +
                     singleInventory["inventoryPath"].get<std::string>()),
                    (IPZ_INTERFACE + (std::string) "." + thisRecord.recName),
                    thisRecord.recKWd, thisRecord.kwdUpdatedData);
            }

            // process Common interface
            processAndUpdateCI(singleInventory["inventoryPath"]
                                   .get_ref<const nlohmann::json::string_t&>());
        }

        if (isInheritEI)
        {
            if (isCpuModuleOnly)
            {
                makeDbusCall<Binary>(
                    (INVENTORY_PATH +
                     singleInventory["inventoryPath"].get<std::string>()),
                    (IPZ_INTERFACE + (std::string) "." + thisRecord.recName),
                    thisRecord.recKWd, thisRecord.kwdUpdatedData);
            }

            // process extra interfaces
            processAndUpdateEI(singleInventory,
                               singleInventory["inventoryPath"]
                                   .get_ref<const nlohmann::json::string_t&>());
        }
    }
}

void EditorImpl::expandLocationCode(const std::string& locationCodeType)
{
    std::string propertyFCorTM{};
    std::string propertySE{};

    if (locationCodeType == "fcs")
    {
        propertyFCorTM =
            readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VCEN", "FC");
        propertySE =
            readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VCEN", "SE");
    }
    else if (locationCodeType == "mts")
    {
        propertyFCorTM =
            readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VSYS", "TM");
        propertySE =
            readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VSYS", "SE");
    }

    const nlohmann::json& groupFRUS =
        jsonFile["frus"].get_ref<const nlohmann::json::object_t&>();
    for (const auto& itemFRUS : groupFRUS.items())
    {
        const std::vector<nlohmann::json>& groupEEPROM =
            itemFRUS.value().get_ref<const nlohmann::json::array_t&>();
        for (const auto& itemEEPROM : groupEEPROM)
        {
            // check if the given item implements location code interface
            if (itemEEPROM["extraInterfaces"].find(LOCATION_CODE_INF) !=
                itemEEPROM["extraInterfaces"].end())
            {
                const std::string& unexpandedLocationCode =
                    itemEEPROM["extraInterfaces"][LOCATION_CODE_INF]
                              ["LocationCode"]
                                  .get_ref<const nlohmann::json::string_t&>();
                std::size_t idx = unexpandedLocationCode.find(locationCodeType);
                if (idx != std::string::npos)
                {
                    std::string expandedLoctionCode(unexpandedLocationCode);

                    if (locationCodeType == "fcs")
                    {
                        expandedLoctionCode.replace(
                            idx, 3,
                            propertyFCorTM.substr(0, 4) + ".ND0." + propertySE);
                    }
                    else if (locationCodeType == "mts")
                    {
                        std::replace(propertyFCorTM.begin(),
                                     propertyFCorTM.end(), '-', '.');
                        expandedLoctionCode.replace(
                            idx, 3, propertyFCorTM + "." + propertySE);
                    }

                    // update the DBUS interface
                    makeDbusCall<std::string>(
                        (INVENTORY_PATH +
                         itemEEPROM["inventoryPath"]
                             .get_ref<const nlohmann::json::string_t&>()),
                        LOCATION_CODE_INF, "LocationCode", expandedLoctionCode);
                }
            }
        }
    }
}

string EditorImpl::getSysPathForThisFruType(const string& moduleObjPath,
                                            const string& fruType)
{
    string fruVpdPath;

    // get all FRUs list
    for (const auto& eachFru : jsonFile["frus"].items())
    {
        bool moduleObjPathMatched = false;
        bool expectedFruFound = false;

        for (const auto& eachInventory : eachFru.value())
        {
            const auto& thisObjectPath = eachInventory["inventoryPath"];

            // "type" exists only in CPU module and FRU
            if (eachInventory.find("type") != eachInventory.end())
            {
                // If inventory type is fruAndModule then set flag
                if (eachInventory["type"] == fruType)
                {
                    expectedFruFound = true;
                }
            }

            if (thisObjectPath == moduleObjPath)
            {
                moduleObjPathMatched = true;
            }
        }

        // If condition satisfies then collect this sys path and exit
        if (expectedFruFound && moduleObjPathMatched)
        {
            fruVpdPath = eachFru.key();
            break;
        }
    }

    return fruVpdPath;
}

void EditorImpl::getVpdPathForCpu()
{
    isCI = false;
    // keep a backup In case we need it later
    inventory::Path vpdFilePathBackup = vpdFilePath;

    // TODO 1:Temp hardcoded list. create it dynamically.
    std::vector<std::string> commonIntVINIKwds = {"PN", "SN", "DR"};
    std::vector<std::string> commonIntVR10Kwds = {"DC"};
    std::unordered_map<std::string, std::vector<std::string>>
        commonIntRecordsList = {{"VINI", commonIntVINIKwds},
                                {"VR10", commonIntVR10Kwds}};

    // If requested Record&Kw is one among CI, then update 'FRU' type sys
    // path, SPI2
    unordered_map<std::string, vector<string>>::const_iterator isCommonInt =
        commonIntRecordsList.find(thisRecord.recName);

    if ((isCommonInt != commonIntRecordsList.end()) &&
        (find(isCommonInt->second.begin(), isCommonInt->second.end(),
              thisRecord.recKWd) != isCommonInt->second.end()))
    {
        isCI = true;
        vpdFilePath = getSysPathForThisFruType(objPath, "fruAndModule");
    }
    else
    {
        for (const auto& eachFru : jsonFile["frus"].items())
        {
            for (const auto& eachInventory : eachFru.value())
            {
                if (eachInventory.find("type") != eachInventory.end())
                {
                    const auto& thisObjectPath = eachInventory["inventoryPath"];
                    if ((eachInventory["type"] == "moduleOnly") &&
                        (eachInventory.value("inheritEI", true)) &&
                        (thisObjectPath == static_cast<string>(objPath)))
                    {
                        vpdFilePath = eachFru.key();
                    }
                }
            }
        }
    }
    // If it is not a CPU fru then go ahead with default vpdFilePath from
    // fruMap
    if (vpdFilePath.empty())
    {
        vpdFilePath = vpdFilePathBackup;
    }
}

void EditorImpl::checkFileValidity(Binary vpd)
{
    if (vpd.empty())
    {
        std::string errorMsg =
            std::string("VPD File \"") + vpdFilePath + ("\" is Empty/Invalid.");
        throw std::runtime_error(errorMsg);
    }
    auto iterator = vpd.cbegin();
    std::advance(iterator, IPZ_DATA_START);

    Byte vpdType = *iterator;
    if (vpdType == KW_VAL_PAIR_START_TAG)
    {
        ParserInterface* Iparser =
            ParserFactory::getParser(std::move(vpd), vpdFilePath);
        IpzVpdParser* ipzParser = dynamic_cast<IpzVpdParser*>(Iparser);

        try
        {
            if (ipzParser == nullptr)
            {
                throw std::runtime_error(
                    "The given vpd is not of type IPZ. Thereby, dynamic cast "
                    "failure occured while tried to downcast a vpdParser "
                    "object to IpzVpdParser object.");
            }

            ipzParser->processHeader();
            delete ipzParser;
            ipzParser = nullptr;

            // process VTOC for PTT rkwd
            readVTOC();

            // check record for keywrod
            checkRecordForKwd();
        }
        catch (const std::exception& e)
        {
            if (ipzParser != nullptr)
            {
                delete ipzParser;
            }
            throw std::runtime_error(e.what());
        }
    }
    return;
}

void EditorImpl::updateKeyword(const Binary& kwdData)
{
    offset = 0;
#ifndef ManagerTest
    getVpdPathForCpu();
    // check if offset present?
    for (const auto& item : jsonFile["frus"][vpdFilePath])
    {
        if (item.find("offset") != item.end())
        {
            offset = item["offset"];
        }
    }
    char buf[2048];
    vpdFileStream.rdbuf()->pubsetbuf(buf, sizeof(buf));
    vpdFileStream.open(vpdFilePath,
                       std::ios::in | std::ios::out | std::ios::binary);
    vpdFileStream.seekg(offset, std::ios_base::cur);

    // Read 64KB data content of the binary file into a vector
    Binary tmpVector((istreambuf_iterator<char>(vpdFileStream)),
                     istreambuf_iterator<char>());

    vector<unsigned char>::const_iterator first = tmpVector.begin();
    vector<unsigned char>::const_iterator last = tmpVector.begin() + NEXT_64_KB;

    vpdFile = tmpVector;
    if (distance(first, last) > static_cast<int>(tmpVector.size()))
    {
        Binary extracted64KbVpd(first, last);
        vpdFile = extracted64KbVpd;
    }
#endif

    // check if the vpd file is valid and it contains the kwd and record
    checkFileValidity(vpdFile);

    // update the data to the file
    updateData(kwdData);

    // update the ECC data for the record once data has been updated
    updateRecordECC();
    if (updCache)
    {
#ifndef ManagerTest
        // update the cache once data has been updated
        updateCache();
#endif
    }
    return;
}

std::string EditorImpl::getKwdData()
{
    // check if the vpd file is valid and check for record and kwd
    checkFileValidity(vpdFile);

    auto iteratorToKWdData = vpdFile.begin();
    std::advance(iteratorToKWdData, thisRecord.kwDataOffset);
    auto end = iteratorToKWdData;
    std::advance(end, thisRecord.kwdDataLength);

    std::string kwdData(iteratorToKWdData, end);

    return kwdData;
}

} // namespace editor
} // namespace manager
} // namespace vpd
} // namespace openpower
