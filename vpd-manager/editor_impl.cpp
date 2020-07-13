#include "config.h"

#include "editor_impl.hpp"

#include "ipz_parser.hpp"
#include "parser_factory.hpp"
#include "utils.hpp"
#include "vpd_exceptions.hpp"

#include "vpdecc/vpdecc.h"

using namespace openpower::vpd::parser::interface;
using namespace openpower::vpd::constants;
using namespace openpower::vpd::parser::factory;
using namespace openpower::vpd::ipz::parser;
using namespace openpower::vpd::exceptions;

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
    throw std::runtime_error("Record not found");
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
    auto iteratorToKWdData = vpdVector.begin();
    std::advance(iteratorToKWdData, thisRecord.kwDataOffset);
    std::copy(iteratorToNewdata, end, iteratorToKWdData);

#ifdef ManagerTest
    auto startItr = vpdVector.begin();
    std::advance(iteratorToKWdData, thisRecord.kwDataOffset);
    auto endItr = startItr;
    std::advance(endItr, thisRecord.kwdDataLength);

    Binary updatedData(startItr, endItr);
    if (updatedData == kwdData)
    {
        throw std::runtime_error("Data updated successfully");
    }
#else

    // update data in EEPROM as well. As we will not write complete file back
    vpdFileStream.seekp(startOffset + thisRecord.kwDataOffset, std::ios::beg);

    iteratorToNewdata = kwdData.cbegin();
    std::copy(iteratorToNewdata, end,
              std::ostreambuf_iterator<char>(vpdFileStream));

    // get a hold to new data in case encoding is needed
    thisRecord.kwdUpdatedData.resize(thisRecord.kwdDataLength);
    auto itrToKWdData = vpdVector.cbegin();
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

    auto iterator = vpdVector.cbegin();
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
            thisRecord.kwDataOffset =
                std::distance(vpdVector.cbegin(), iterator);
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
    auto itrToRecordData = vpdVector.cbegin();
    std::advance(itrToRecordData, thisRecord.recOffset);

    auto itrToRecordECC = vpdVector.cbegin();
    std::advance(itrToRecordECC, thisRecord.recECCoffset);

    auto l_status = vpdecc_create_ecc(
        const_cast<uint8_t*>(&itrToRecordData[0]), thisRecord.recSize,
        const_cast<uint8_t*>(&itrToRecordECC[0]), &thisRecord.recECCLength);
    if (l_status != VPD_ECC_OK)
    {
        string str = "Ecc update failed for the given record ";
        str += thisRecord.recName;
        str += " ,	present in the given object ";
        str += objPath;
        str += ".";
        throw VpdEccException(str);
    }
    else
    {
        cout << "\n ECC updation successfull for the given record "
             << thisRecord.recName << " ,present in the given object "
             << objPath << "." << endl;
    }

    auto end = itrToRecordECC;
    std::advance(end, thisRecord.recECCLength);

#ifndef ManagerTest
    vpdFileStream.seekp(startOffset + thisRecord.recECCoffset, std::ios::beg);
    std::copy(itrToRecordECC, end,
              std::ostreambuf_iterator<char>(vpdFileStream));
#endif
}

auto EditorImpl::getValue(offsets::Offsets offset)
{
    auto itr = vpdVector.cbegin();
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

    auto itrToRecord = vpdVector.cbegin();
    std::advance(itrToRecord, tocOffset);

    auto iteratorToECC = vpdVector.cbegin();
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
        throw std::runtime_error("VTOC record not found");
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
        throw std::runtime_error("bus call failed");
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
    string fruVpdPath{};
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

void EditorImpl::getVpdPathForCpu(bool ecc)
{
    if (vpdFilePath.find("spi") != string::npos)
    {
        isCI = false;
        inventory::Path vpdFilePathLocal{};
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
                  thisRecord.recKWd) != isCommonInt->second.end()) &&
            (!ecc))
        {
            isCI = true;
            vpdFilePathLocal =
                getSysPathForThisFruType(objPath, "fruAndModule");
        }
        else
        {
            for (const auto& eachFru : jsonFile["frus"].items())
            {
                for (const auto& eachInventory : eachFru.value())
                {
                    if (eachInventory.find("type") != eachInventory.end())
                    {
                        const auto& thisObjectPath =
                            eachInventory["inventoryPath"];
                        if ((eachInventory["type"] == "moduleOnly") &&
                            (eachInventory.value("inheritEI", true)) &&
                            (thisObjectPath == objPath))
                        {
                            vpdFilePathLocal = eachFru.key();
                            break;
                        }
                        else if ((ecc) &&
                                 (eachInventory["type"] == "fruAndModule") &&
                                 (thisObjectPath == objPath))
                        {
                            vpdFilePathLocal = eachFru.key();
                            break;
                        }
                    }
                }
                if (!vpdFilePathLocal.empty())
                {
                    vpdFilePath = vpdFilePathLocal;
                    break;
                }
            }
        }
    }
}

void EditorImpl::updateKeyword(const Binary& kwdData)
{
    startOffset = 0;
#ifndef ManagerTest
    getParsedInventoryJsonObject(jsonFile);
    getVpdPathForCpu(false);
    getVpdDataInVector(jsonFile, vpdFilePath, vpdFileStream, startOffset,
                       vpdVector);
#endif
    if (vpdVector.empty())
    {
        throw std::runtime_error("Invalid File");
    }
    auto iterator = vpdVector.cbegin();
    std::advance(iterator, IPZ_DATA_START);

    Byte vpdType = *iterator;
    Binary vpdVectorLocal = vpdVector;
    if (vpdType == KW_VAL_PAIR_START_TAG)
    {
        ParserInterface* Iparser =
            ParserFactory::getParser(std::move(vpdVectorLocal));
        IpzVpdParser* ipzParser = dynamic_cast<IpzVpdParser*>(Iparser);

        try
        {
            if (ipzParser == nullptr)
            {
                throw std::runtime_error("Invalid cast");
            }

            ipzParser->processHeader();
            delete ipzParser;
            ipzParser = nullptr;
            // ParserFactory::freeParser(Iparser);

            // process VTOC for PTT rkwd
            readVTOC();

            // check record for keywrod
            checkRecordForKwd();

            // update the data to the file
            updateData(kwdData);

            // update the ECC data for the record once data has been updated
            updateRecordECC();
#ifndef ManagerTest
            // update the cache once data has been updated
            updateCache();
#endif
        }
        catch (const std::exception& e)
        {
            if (ipzParser != nullptr)
            {
                delete ipzParser;
            }
            throw std::runtime_error(e.what());
        }
        return;
    }
}

void EditorImpl::fixBrokenEcc()
{
    try
    {
        getParsedInventoryJsonObject(jsonFile);
        inventory::FrusMap frus{};
        getInvToEepromMap(frus, jsonFile);
        if (frus.find(objPath) == frus.end())
        {
            throw std::runtime_error("Inventory path not found");
        }
        vpdFilePath = (frus.find(objPath)->second).first;
        getVpdPathForCpu(true);
        for (const auto& item : jsonFile["frus"][vpdFilePath])
        {
            if (item.find("offset") != item.end())
            {
                startOffset = item["offset"];
            }
        }
        getVpdDataInVector(jsonFile, vpdFilePath, vpdFileStream, startOffset,
                           vpdVector);

        Binary vpdVectorLocal = vpdVector;
        ParserInterface* Iparser =
            ParserFactory::getParser(std::move(vpdVectorLocal));
        IpzVpdParser* ipzParser = dynamic_cast<IpzVpdParser*>(Iparser);
        try
        {
            if (ipzParser == nullptr)
            {
                throw std::runtime_error("Invalid cast");
            }

            ipzParser->processHeader();
            delete ipzParser;
            ipzParser = nullptr;

            readVTOC();
            updateRecordECC();
        }
        catch (const std::exception& e)
        {
            if (ipzParser != nullptr)
            {
                delete ipzParser;
            }
            throw std::runtime_error(e.what());
        }
        vpdFileStream.close();
        delete ipzParser;
        ipzParser = nullptr;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}
} // namespace editor
} // namespace manager
} // namespace vpd
} // namespace openpower
