#include "config.h"

#include "worker.hpp"

#include "backup_restore.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include "parser.hpp"
#include "parser_factory.hpp"
#include "parser_interface.hpp"

#include <utility/common_utility.hpp>
#include <utility/dbus_utility.hpp>
#include <utility/event_logger_utility.hpp>
#include <utility/json_utility.hpp>
#include <utility/vpd_specific_utility.hpp>

#include <filesystem>
#include <fstream>
#include <future>
#include <typeindex>
#include <unordered_set>

namespace vpd
{

Worker::Worker(std::string pathToConfigJson, uint8_t i_maxThreadCount,
               types::VpdCollectionMode i_vpdCollectionMode) :
    m_configJsonPath(pathToConfigJson), m_semaphore(i_maxThreadCount),
    m_vpdCollectionMode(i_vpdCollectionMode),
    m_logger(Logger::getLoggerInstance())
{
    // Implies the processing is based on some config JSON
    if (!m_configJsonPath.empty())
    {
        uint16_t l_errCode = 0;
        m_parsedJson = jsonUtility::getParsedJson(m_configJsonPath, l_errCode);

        if (l_errCode)
        {
            throw JsonException("JSON parsing failed. error : " +
                                    commonUtility::getErrCodeMsg(l_errCode),
                                m_configJsonPath);
        }

        // check for mandatory fields at this point itself.
        if (!m_parsedJson.contains("frus"))
        {
            throw JsonException("Mandatory tag(s) missing from JSON",
                                m_configJsonPath);
        }
    }
    else
    {
        logging::logMessage("Processing in not based on any config JSON");
    }
}

void Worker::populateIPZVPDpropertyMap(
    types::InterfaceMap& interfacePropMap,
    const types::IPZKwdValueMap& keyordValueMap,
    const std::string& interfaceName)
{
    types::PropertyMap propertyValueMap;
    for (const auto& kwdVal : keyordValueMap)
    {
        auto kwd = kwdVal.first;

        if (kwd[0] == '#')
        {
            kwd = std::string("PD_") + kwd[1];
        }
        else if (isdigit(kwd[0]))
        {
            kwd = std::string("N_") + kwd;
        }

        types::BinaryVector value(kwdVal.second.begin(), kwdVal.second.end());
        propertyValueMap.emplace(move(kwd), move(value));
    }

    if (!propertyValueMap.empty())
    {
        interfacePropMap.emplace(interfaceName, propertyValueMap);
    }
}

void Worker::populateKwdVPDpropertyMap(const types::KeywordVpdMap& keyordVPDMap,
                                       types::InterfaceMap& interfaceMap)
{
    for (const auto& kwdValMap : keyordVPDMap)
    {
        types::PropertyMap propertyValueMap;
        auto kwd = kwdValMap.first;

        if (kwd[0] == '#')
        {
            kwd = std::string("PD_") + kwd[1];
        }
        else if (isdigit(kwd[0]))
        {
            kwd = std::string("N_") + kwd;
        }

        if (auto keywordValue = get_if<types::BinaryVector>(&kwdValMap.second))
        {
            types::BinaryVector value((*keywordValue).begin(),
                                      (*keywordValue).end());
            propertyValueMap.emplace(move(kwd), move(value));
        }
        else if (auto keywordValue = get_if<std::string>(&kwdValMap.second))
        {
            types::BinaryVector value((*keywordValue).begin(),
                                      (*keywordValue).end());
            propertyValueMap.emplace(move(kwd), move(value));
        }
        else if (auto keywordValue = get_if<size_t>(&kwdValMap.second))
        {
            if (kwd == "MemorySizeInKB")
            {
                types::PropertyMap memProp;
                memProp.emplace(move(kwd), ((*keywordValue)));
                interfaceMap.emplace("xyz.openbmc_project.Inventory.Item.Dimm",
                                     move(memProp));
                continue;
            }
            else
            {
                logging::logMessage(
                    "Unknown Keyword =" + kwd + " found in keyword VPD map");
                continue;
            }
        }
        else
        {
            logging::logMessage(
                "Unknown variant type found in keyword VPD map.");
            continue;
        }

        if (!propertyValueMap.empty())
        {
            uint16_t l_errCode = 0;
            vpdSpecificUtility::insertOrMerge(
                interfaceMap, constants::kwdVpdInf, move(propertyValueMap),
                l_errCode);

            if (l_errCode)
            {
                logging::logMessage(
                    "Failed to insert value into map, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }
        }
    }
}

void Worker::populateInterfaces(const nlohmann::json& interfaceJson,
                                types::InterfaceMap& interfaceMap,
                                const types::VPDMapVariant& parsedVpdMap)
{
    for (const auto& interfacesPropPair : interfaceJson.items())
    {
        const std::string& interface = interfacesPropPair.key();
        types::PropertyMap propertyMap;
        uint16_t l_errCode = 0;

        for (const auto& propValuePair : interfacesPropPair.value().items())
        {
            const std::string property = propValuePair.key();

            if (propValuePair.value().is_boolean())
            {
                propertyMap.emplace(property,
                                    propValuePair.value().get<bool>());
            }
            else if (propValuePair.value().is_string())
            {
                if (property.compare("LocationCode") == 0 &&
                    interface.compare("com.ibm.ipzvpd.Location") == 0)
                {
                    std::string value =
                        vpdSpecificUtility::getExpandedLocationCode(
                            propValuePair.value().get<std::string>(),
                            parsedVpdMap, l_errCode);

                    if (l_errCode)
                    {
                        logging::logMessage(
                            "Failed to get expanded location code for location code - " +
                            propValuePair.value().get<std::string>() +
                            " ,error : " +
                            commonUtility::getErrCodeMsg(l_errCode));
                    }

                    propertyMap.emplace(property, value);

                    auto l_locCodeProperty = propertyMap;
                    vpdSpecificUtility::insertOrMerge(
                        interfaceMap,
                        std::string(constants::xyzLocationCodeInf),
                        move(l_locCodeProperty), l_errCode);

                    if (l_errCode)
                    {
                        logging::logMessage(
                            "Failed to insert value into map, error : " +
                            commonUtility::getErrCodeMsg(l_errCode));
                    }
                }
                else
                {
                    propertyMap.emplace(
                        property, propValuePair.value().get<std::string>());
                }
            }
            else if (propValuePair.value().is_array())
            {
                try
                {
                    propertyMap.emplace(
                        property,
                        propValuePair.value().get<types::BinaryVector>());
                }
                catch (const nlohmann::detail::type_error& e)
                {
                    std::cerr << "Type exception: " << e.what() << "\n";
                }
            }
            else if (propValuePair.value().is_number())
            {
                // For now assume the value is a size_t.  In the future it would
                // be nice to come up with a way to get the type from the JSON.
                propertyMap.emplace(property,
                                    propValuePair.value().get<size_t>());
            }
            else if (propValuePair.value().is_object())
            {
                const std::string& record =
                    propValuePair.value().value("recordName", "");
                const std::string& keyword =
                    propValuePair.value().value("keywordName", "");
                const std::string& encoding =
                    propValuePair.value().value("encoding", "");

                if (auto ipzVpdMap =
                        std::get_if<types::IPZVpdMap>(&parsedVpdMap))
                {
                    if (!record.empty() && !keyword.empty() &&
                        (*ipzVpdMap).count(record) &&
                        (*ipzVpdMap).at(record).count(keyword))
                    {
                        auto encoded = vpdSpecificUtility::encodeKeyword(
                            ((*ipzVpdMap).at(record).at(keyword)), encoding,
                            l_errCode);

                        if (l_errCode)
                        {
                            logging::logMessage(
                                std::string(
                                    "Failed to get encoded keyword value for : ") +
                                keyword + std::string(", error : ") +
                                commonUtility::getErrCodeMsg(l_errCode));
                        }

                        propertyMap.emplace(property, encoded);
                    }
                }
                else if (auto kwdVpdMap =
                             std::get_if<types::KeywordVpdMap>(&parsedVpdMap))
                {
                    if (!keyword.empty() && (*kwdVpdMap).count(keyword))
                    {
                        if (auto kwValue = std::get_if<types::BinaryVector>(
                                &(*kwdVpdMap).at(keyword)))
                        {
                            auto encodedValue =
                                vpdSpecificUtility::encodeKeyword(
                                    std::string((*kwValue).begin(),
                                                (*kwValue).end()),
                                    encoding, l_errCode);

                            if (l_errCode)
                            {
                                logging::logMessage(
                                    std::string(
                                        "Failed to get encoded keyword value for : ") +
                                    keyword + std::string(", error : ") +
                                    commonUtility::getErrCodeMsg(l_errCode));
                            }

                            propertyMap.emplace(property, encodedValue);
                        }
                        else if (auto kwValue = std::get_if<std::string>(
                                     &(*kwdVpdMap).at(keyword)))
                        {
                            auto encodedValue =
                                vpdSpecificUtility::encodeKeyword(
                                    std::string((*kwValue).begin(),
                                                (*kwValue).end()),
                                    encoding, l_errCode);

                            if (l_errCode)
                            {
                                logging::logMessage(
                                    "Failed to get encoded keyword value for : " +
                                    keyword + ", error : " +
                                    commonUtility::getErrCodeMsg(l_errCode));
                            }

                            propertyMap.emplace(property, encodedValue);
                        }
                        else if (auto uintValue = std::get_if<size_t>(
                                     &(*kwdVpdMap).at(keyword)))
                        {
                            propertyMap.emplace(property, *uintValue);
                        }
                        else
                        {
                            logging::logMessage(
                                "Unknown keyword found, Keywrod = " + keyword);
                        }
                    }
                }
            }
        }
        vpdSpecificUtility::insertOrMerge(interfaceMap, interface,
                                          move(propertyMap), l_errCode);

        if (l_errCode)
        {
            logging::logMessage("Failed to insert value into map, error : " +
                                commonUtility::getErrCodeMsg(l_errCode));
        }
    }
}

bool Worker::isCPUIOGoodOnly(const std::string& i_pgKeyword)
{
    const unsigned char l_io[] = {
        0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF,
        0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF, 0xE7, 0xF9, 0xFF};

    // EQ0 index (in PG keyword) starts at 97 (with offset starting from 0).
    // Each EQ carries 3 bytes of data. Totally there are 8 EQs. If all EQs'
    // value equals 0xE7F9FF, then the cpu has no good cores and its treated as
    // IO.
    if (memcmp(l_io, i_pgKeyword.data() + constants::INDEX_OF_EQ0_IN_PG,
               constants::SIZE_OF_8EQ_IN_PG) == 0)
    {
        return true;
    }

    // The CPU is not an IO
    return false;
}

void Worker::processEmbeddedAndSynthesizedFrus(const nlohmann::json& singleFru,
                                               types::InterfaceMap& interfaces)
{
    // embedded property(true or false) says whether the subfru is embedded
    // into the parent fru (or) not. VPD sets Present property only for
    // embedded frus. If the subfru is not an embedded FRU, the subfru may
    // or may not be physically present. Those non embedded frus will always
    // have Present=false irrespective of its physical presence or absence.
    // Eg: nvme drive in nvme slot is not an embedded FRU. So don't set
    // Present to true for such sub frus.
    // Eg: ethernet port is embedded into bmc card. So set Present to true
    // for such sub frus. Also donot populate present property for embedded
    // subfru which is synthesized. Currently there is no subfru which are
    // both embedded and synthesized. But still the case is handled here.

    // Check if its required to handle presence for this FRU.
    if (singleFru.value("handlePresence", true))
    {
        uint16_t l_errCode = 0;
        types::PropertyMap presProp;
        presProp.emplace("Present", true);
        vpdSpecificUtility::insertOrMerge(interfaces,
                                          "xyz.openbmc_project.Inventory.Item",
                                          move(presProp), l_errCode);

        if (l_errCode)
        {
            logging::logMessage("Failed to insert value into map, error : " +
                                commonUtility::getErrCodeMsg(l_errCode));
        }
    }
}

void Worker::processExtraInterfaces(const nlohmann::json& singleFru,
                                    types::InterfaceMap& interfaces,
                                    const types::VPDMapVariant& parsedVpdMap)
{
    populateInterfaces(singleFru["extraInterfaces"], interfaces, parsedVpdMap);
    if (auto ipzVpdMap = std::get_if<types::IPZVpdMap>(&parsedVpdMap))
    {
        if (singleFru["extraInterfaces"].contains(
                "xyz.openbmc_project.Inventory.Item.Cpu"))
        {
            auto itrToRec = (*ipzVpdMap).find("CP00");
            if (itrToRec == (*ipzVpdMap).end())
            {
                return;
            }

            uint16_t l_errCode = 0;
            const std::string pgKeywordValue{vpdSpecificUtility::getKwVal(
                itrToRec->second, "PG", l_errCode)};

            if (!pgKeywordValue.empty())
            {
                if (isCPUIOGoodOnly(pgKeywordValue))
                {
                    interfaces["xyz.openbmc_project.Inventory.Item"]
                              ["PrettyName"] = "IO Module";
                }
            }
            else
            {
                throw DataException(
                    std::string(__FUNCTION__) +
                    "Failed to get value for keyword PG, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }
        }
    }
}

void Worker::processCopyRecordFlag(const nlohmann::json& singleFru,
                                   const types::VPDMapVariant& parsedVpdMap,
                                   types::InterfaceMap& interfaces)
{
    if (auto ipzVpdMap = std::get_if<types::IPZVpdMap>(&parsedVpdMap))
    {
        for (const auto& record : singleFru["copyRecords"])
        {
            const std::string& recordName = record;
            if ((*ipzVpdMap).find(recordName) != (*ipzVpdMap).end())
            {
                populateIPZVPDpropertyMap(interfaces,
                                          (*ipzVpdMap).at(recordName),
                                          constants::ipzVpdInf + recordName);
            }
        }
    }
}

void Worker::processInheritFlag(const types::VPDMapVariant& parsedVpdMap,
                                types::InterfaceMap& interfaces)
{
    if (auto ipzVpdMap = std::get_if<types::IPZVpdMap>(&parsedVpdMap))
    {
        for (const auto& [recordName, kwdValueMap] : *ipzVpdMap)
        {
            populateIPZVPDpropertyMap(interfaces, kwdValueMap,
                                      constants::ipzVpdInf + recordName);
        }
    }
    else if (auto kwdVpdMap = std::get_if<types::KeywordVpdMap>(&parsedVpdMap))
    {
        populateKwdVPDpropertyMap(*kwdVpdMap, interfaces);
    }

    if (m_parsedJson.contains("commonInterfaces"))
    {
        populateInterfaces(m_parsedJson["commonInterfaces"], interfaces,
                           parsedVpdMap);
    }
}

bool Worker::processFruWithCCIN(const nlohmann::json& singleFru,
                                const types::VPDMapVariant& parsedVpdMap)
{
    if (auto ipzVPDMap = std::get_if<types::IPZVpdMap>(&parsedVpdMap))
    {
        auto itrToRec = (*ipzVPDMap).find("VINI");
        if (itrToRec == (*ipzVPDMap).end())
        {
            return false;
        }

        uint16_t l_errCode = 0;
        std::string ccinFromVpd{
            vpdSpecificUtility::getKwVal(itrToRec->second, "CC", l_errCode)};

        if (ccinFromVpd.empty())
        {
            logging::logMessage("Failed to get CCIN kwd value, error : " +
                                commonUtility::getErrCodeMsg(l_errCode));
            return false;
        }

        transform(ccinFromVpd.begin(), ccinFromVpd.end(), ccinFromVpd.begin(),
                  ::toupper);

        std::vector<std::string> ccinList;
        for (std::string ccin : singleFru["ccin"])
        {
            transform(ccin.begin(), ccin.end(), ccin.begin(), ::toupper);
            ccinList.push_back(ccin);
        }

        if (ccinList.empty())
        {
            return false;
        }

        if (find(ccinList.begin(), ccinList.end(), ccinFromVpd) ==
            ccinList.end())
        {
            return false;
        }
    }
    return true;
}

void Worker::processFunctionalProperty(const std::string& i_inventoryObjPath,
                                       types::InterfaceMap& io_interfaces)
{
    if (!dbusUtility::isChassisPowerOn())
    {
        std::vector<std::string> l_operationalStatusInf = {
            constants::operationalStatusInf};

        auto mapperObjectMap = dbusUtility::getObjectMap(
            i_inventoryObjPath, l_operationalStatusInf);

        // If the object has been found. Check if it is under PIM.
        if (mapperObjectMap.size() != 0)
        {
            for (const auto& [l_serviceName, l_interfaceLsit] : mapperObjectMap)
            {
                if (l_serviceName == constants::pimServiceName)
                {
                    // The object is already under PIM. No need to process
                    // again. Retain the old value.
                    return;
                }
            }
        }

        // Implies value is not there in D-Bus. Populate it with default
        // value "true".
        uint16_t l_errCode = 0;
        types::PropertyMap l_functionalProp;
        l_functionalProp.emplace("Functional", true);
        vpdSpecificUtility::insertOrMerge(io_interfaces,
                                          constants::operationalStatusInf,
                                          move(l_functionalProp), l_errCode);

        if (l_errCode)
        {
            logging::logMessage(
                "Failed to insert interface into map, error : " +
                commonUtility::getErrCodeMsg(l_errCode));
        }
    }

    // if chassis is power on. Functional property should be there on D-Bus.
    // Don't process.
    return;
}

void Worker::processEnabledProperty(const std::string& i_inventoryObjPath,
                                    types::InterfaceMap& io_interfaces)
{
    if (!dbusUtility::isChassisPowerOn())
    {
        std::vector<std::string> l_enableInf = {constants::enableInf};

        auto mapperObjectMap =
            dbusUtility::getObjectMap(i_inventoryObjPath, l_enableInf);

        // If the object has been found. Check if it is under PIM.
        if (mapperObjectMap.size() != 0)
        {
            for (const auto& [l_serviceName, l_interfaceLsit] : mapperObjectMap)
            {
                if (l_serviceName == constants::pimServiceName)
                {
                    // The object is already under PIM. No need to process
                    // again. Retain the old value.
                    return;
                }
            }
        }

        // Implies value is not there in D-Bus. Populate it with default
        // value "true".
        uint16_t l_errCode = 0;
        types::PropertyMap l_enabledProp;
        l_enabledProp.emplace("Enabled", true);
        vpdSpecificUtility::insertOrMerge(io_interfaces, constants::enableInf,
                                          move(l_enabledProp), l_errCode);

        if (l_errCode)
        {
            logging::logMessage(
                "Failed to insert interface into map, error : " +
                commonUtility::getErrCodeMsg(l_errCode));
        }
    }

    // if chassis is power on. Enabled property should be there on D-Bus.
    // Don't process.
    return;
}

void Worker::populateDbus(const types::VPDMapVariant& parsedVpdMap,
                          types::ObjectMap& objectInterfaceMap,
                          const std::string& vpdFilePath)
{
    if (vpdFilePath.empty())
    {
        throw std::runtime_error(
            std::string(__FUNCTION__) +
            "Invalid parameter passed to populateDbus API.");
    }

    // JSON config is mandatory for processing of "if". Add "else" for any
    // processing without config JSON.
    if (!m_parsedJson.empty())
    {
        types::InterfaceMap interfaces;

        for (const auto& aFru : m_parsedJson["frus"][vpdFilePath])
        {
            const auto& inventoryPath = aFru["inventoryPath"];
            sdbusplus::message::object_path fruObjectPath(inventoryPath);
            if (aFru.contains("ccin"))
            {
                if (!processFruWithCCIN(aFru, parsedVpdMap))
                {
                    continue;
                }
            }

            if (aFru.value("inherit", true))
            {
                processInheritFlag(parsedVpdMap, interfaces);
            }

            // If specific record needs to be copied.
            if (aFru.contains("copyRecords"))
            {
                processCopyRecordFlag(aFru, parsedVpdMap, interfaces);
            }

            if (aFru.contains("extraInterfaces"))
            {
                // Process extra interfaces w.r.t a FRU.
                processExtraInterfaces(aFru, interfaces, parsedVpdMap);
            }

            // Process FRUS which are embedded in the parent FRU and whose VPD
            // will be synthesized.
            if ((aFru.value("embedded", true)) &&
                (!aFru.value("synthesized", false)))
            {
                processEmbeddedAndSynthesizedFrus(aFru, interfaces);
            }

            processFunctionalProperty(inventoryPath, interfaces);
            processEnabledProperty(inventoryPath, interfaces);

            objectInterfaceMap.emplace(std::move(fruObjectPath),
                                       std::move(interfaces));
        }
    }
}

bool Worker::processPreAction(const std::string& i_vpdFilePath,
                              const std::string& i_flagToProcess,
                              uint16_t& i_errCode)
{
    i_errCode = 0;
    if (i_vpdFilePath.empty() || i_flagToProcess.empty())
    {
        i_errCode = error_code::INVALID_INPUT_PARAMETER;
        return false;
    }

    if ((!jsonUtility::executeBaseAction(m_parsedJson, "preAction",
                                         i_vpdFilePath, i_flagToProcess,
                                         i_errCode)) &&
        (i_flagToProcess.compare("collection") == constants::STR_CMP_SUCCESS))
    {
        // TODO: Need a way to delete inventory object from Dbus and persisted
        // data section in case any FRU is not present or there is any
        // problem in collecting it. Once it has been deleted, it can be
        // re-created in the flow of priming the inventory. This needs to be
        // done either here or in the exception section of "parseAndPublishVPD"
        // API. Any failure in the process of collecting FRU will land up in the
        // excpetion of "parseAndPublishVPD".

        // If the FRU is not there, clear the VINI/CCIN data.
        // Enity manager probes for this keyword to look for this
        // FRU, now if the data is persistent on BMC and FRU is
        // removed this can lead to ambiguity. Hence clearing this
        // Keyword if FRU is absent.
        const auto& inventoryPath =
            m_parsedJson["frus"][i_vpdFilePath].at(0).value("inventoryPath",
                                                            "");

        if (!inventoryPath.empty())
        {
            types::ObjectMap l_pimObjMap{
                {inventoryPath,
                 {{constants::kwdVpdInf,
                   {{constants::kwdCCIN, types::BinaryVector{}}}}}}};

            if (!dbusUtility::callPIM(std::move(l_pimObjMap)))
            {
                logging::logMessage(
                    "Call to PIM failed for file " + i_vpdFilePath);
            }
        }
        else
        {
            logging::logMessage(
                "Inventory path is empty in Json for file " + i_vpdFilePath);
        }

        return false;
    }
    return true;
}

bool Worker::processPostAction(
    const std::string& i_vpdFruPath, const std::string& i_flagToProcess,
    const std::optional<types::VPDMapVariant> i_parsedVpd)
{
    if (i_vpdFruPath.empty() || i_flagToProcess.empty())
    {
        logging::logMessage(
            "Invalid input parameter. Abort processing post action");
        return false;
    }

    // Check if post action tag is to be triggered in the flow of collection
    // based on some CCIN value?
    uint16_t l_errCode = 0;

    if (m_parsedJson["frus"][i_vpdFruPath]
            .at(0)["postAction"][i_flagToProcess]
            .contains("ccin"))
    {
        if (!i_parsedVpd.has_value())
        {
            logging::logMessage("Empty VPD Map");
            return false;
        }

        // CCIN match is required to process post action for this FRU as it
        // contains the flag.
        if (!vpdSpecificUtility::findCcinInVpd(
                m_parsedJson["frus"][i_vpdFruPath].at(
                    0)["postAction"]["collection"],
                i_parsedVpd.value(), l_errCode))
        {
            if (l_errCode)
            {
                // ToDo - Check if PEL is required in case of RECORD_NOT_FOUND
                // and KEYWORD_NOT_FOUND error codes.
                logging::logMessage("Failed to find CCIN in VPD, error : " +
                                    commonUtility::getErrCodeMsg(l_errCode));
            }

            // If CCIN is not found, implies post action processing is not
            // required for this FRU. Let the flow continue.
            return true;
        }
    }

    if (!jsonUtility::executeBaseAction(m_parsedJson, "postAction",
                                        i_vpdFruPath, i_flagToProcess,
                                        l_errCode))
    {
        logging::logMessage(
            "Execution of post action failed for path: " + i_vpdFruPath +
            " . Reason: " + commonUtility::getErrCodeMsg(l_errCode));

        // If post action was required and failed only in that case return
        // false. In all other case post action is considered passed.
        return false;
    }

    return true;
}

types::VPDMapVariant Worker::parseVpdFile(const std::string& i_vpdFilePath)
{
    try
    {
        uint16_t l_errCode = 0;

        if (i_vpdFilePath.empty())
        {
            throw std::runtime_error(
                std::string(__FUNCTION__) +
                " Empty VPD file path passed. Abort processing");
        }

        bool isPreActionRequired = false;
        if (jsonUtility::isActionRequired(m_parsedJson, i_vpdFilePath,
                                          "preAction", "collection", l_errCode))
        {
            isPreActionRequired = true;
            if (!processPreAction(i_vpdFilePath, "collection", l_errCode))
            {
                if (l_errCode == error_code::DEVICE_NOT_PRESENT)
                {
                    logging::logMessage(
                        commonUtility::getErrCodeMsg(l_errCode) +
                        i_vpdFilePath);

                    // since pre action is reporting device not present, execute
                    // post fail action
                    checkAndExecutePostFailAction(i_vpdFilePath, "collection");

                    // Presence pin has been read successfully and has been read
                    // as false, so this is not a failure case, hence returning
                    // empty variant so that pre action is not marked as failed.
                    return types::VPDMapVariant{};
                }
                throw std::runtime_error(
                    std::string(__FUNCTION__) +
                    " Pre-Action failed with error: " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }
        }
        else if (l_errCode)
        {
            logging::logMessage(
                "Failed to check if pre action required for FRU [" +
                i_vpdFilePath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        if (!std::filesystem::exists(i_vpdFilePath))
        {
            if (isPreActionRequired)
            {
                throw std::runtime_error(
                    std::string(__FUNCTION__) + " Could not find file path " +
                    i_vpdFilePath + "Skipping parser trigger for the EEPROM");
            }
            return types::VPDMapVariant{};
        }

        std::shared_ptr<Parser> vpdParser =
            std::make_shared<Parser>(i_vpdFilePath, m_parsedJson);

        types::VPDMapVariant l_parsedVpd = vpdParser->parse();

        // Before returning, as collection is over, check if FRU qualifies for
        // any post action in the flow of collection.
        // Note: Don't change the order, post action needs to be processed only
        // after collection for FRU is successfully done.
        if (jsonUtility::isActionRequired(m_parsedJson, i_vpdFilePath,
                                          "postAction", "collection",
                                          l_errCode))
        {
            if (!processPostAction(i_vpdFilePath, "collection", l_parsedVpd))
            {
                // Post action was required but failed while executing.
                // Behaviour can be undefined.
                EventLogger::createSyncPel(
                    types::ErrorType::InternalFailure,
                    types::SeverityType::Warning, __FILE__, __FUNCTION__, 0,
                    std::string("Required post action failed for path [" +
                                i_vpdFilePath + "]"),
                    std::nullopt, std::nullopt, std::nullopt, std::nullopt);
            }
        }
        else if (l_errCode)
        {
            logging::logMessage(
                "Error while checking if post action required for FRU [" +
                i_vpdFilePath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        return l_parsedVpd;
    }
    catch (std::exception& l_ex)
    {
        std::string l_exMsg{
            std::string(__FUNCTION__) + " : VPD parsing failed for " +
            i_vpdFilePath + " due to error: " + l_ex.what()};

        // If post fail action is required, execute it.
        checkAndExecutePostFailAction(i_vpdFilePath, "collection");

        if (typeid(l_ex) == typeid(DataException))
        {
            throw DataException(l_exMsg);
        }
        else if (typeid(l_ex) == typeid(EccException))
        {
            throw EccException(l_exMsg);
        }
        throw std::runtime_error(l_exMsg);
    }
}

std::tuple<bool, std::string> Worker::parseAndPublishVPD(
    const std::string& i_vpdFilePath)
{
    std::string l_inventoryPath{};
    uint16_t l_errCode = 0;
    try
    {
        m_semaphore.acquire();

        // Thread launched.
        m_mutex.lock();
        m_activeCollectionThreadCount++;
        m_mutex.unlock();

        vpdSpecificUtility::setCollectionStatusProperty(
            i_vpdFilePath, types::VpdCollectionStatus::InProgress, m_parsedJson,
            l_errCode);
        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status for path " + i_vpdFilePath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        const types::VPDMapVariant& parsedVpdMap = parseVpdFile(i_vpdFilePath);
        if (!std::holds_alternative<std::monostate>(parsedVpdMap))
        {
            types::ObjectMap objectInterfaceMap;
            populateDbus(parsedVpdMap, objectInterfaceMap, i_vpdFilePath);

            // Notify PIM
            if (!dbusUtility::callPIM(move(objectInterfaceMap)))
            {
                throw std::runtime_error(
                    std::string(__FUNCTION__) +
                    "Call to PIM failed while publishing VPD.");
            }
        }
        else
        {
            // Stale data from the previous boot can be present on the system.
            // so clearing of data incase of empty map received.
            // As empty parsedVpdMap recieved for some reason, but still
            // considered VPD collection is completed. Hence FRU collection
            // Status will be set as completed.

            vpdSpecificUtility::resetObjTreeVpd(i_vpdFilePath, m_parsedJson,
                                                l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to reset data under PIM for path [" +
                    i_vpdFilePath +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }

            m_logger->logMessage("Empty parsedVpdMap recieved for path [" +
                                     i_vpdFilePath + "]. Check PEL for reason.",
                                 PlaceHolder::COLLECTION);
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            i_vpdFilePath, types::VpdCollectionStatus::Completed, m_parsedJson,
            l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status as completed for path " +
                i_vpdFilePath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        m_semaphore.release();
        return std::make_tuple(true, i_vpdFilePath);
    }
    catch (const std::exception& ex)
    {
        uint16_t l_errCode = 0;

        // stale data can be present on the system from previous boot. so
        // clearing of data in case of failure.
        vpdSpecificUtility::resetObjTreeVpd(i_vpdFilePath, m_parsedJson,
                                            l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to reset under PIM for path [" + i_vpdFilePath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            i_vpdFilePath, types::VpdCollectionStatus::Failed, m_parsedJson,
            l_errCode);
        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status as failed for path " +
                i_vpdFilePath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        // handle all the exceptions internally. Return only true/false
        // based on status of execution.
        if (typeid(ex) == std::type_index(typeid(DataException)))
        {
            // In case of pass1 planar, VPD can be corrupted on PCIe cards. Skip
            // logging error for these cases.
            if (vpdSpecificUtility::isPass1Planar(l_errCode))
            {
                std::string l_invPath =
                    jsonUtility::getInventoryObjPathFromJson(
                        m_parsedJson, i_vpdFilePath, l_errCode);

                if (l_errCode != 0)
                {
                    m_logger->logMessage(
                        "Failed to get inventory object path from JSON for FRU [" +
                            i_vpdFilePath + "], error: " +
                            commonUtility::getErrCodeMsg(l_errCode),
                        PlaceHolder::COLLECTION);
                }

                const std::string& l_invPathLeafValue =
                    sdbusplus::message::object_path(l_invPath).filename();

                if ((l_invPathLeafValue.find("pcie_card", 0) !=
                     std::string::npos))
                {
                    // skip logging any PEL for PCIe cards on pass 1 planar.
                    return std::make_tuple(false, i_vpdFilePath);
                }
            }
            else if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to check if system is Pass 1 Planar, error : " +
                        commonUtility::getErrCodeMsg(l_errCode),
                    PlaceHolder::COLLECTION);
            }
        }

        EventLogger::createSyncPel(
            EventLogger::getErrorType(ex),
            (typeid(ex) == typeid(DataException)) ||
                    (typeid(ex) == typeid(EccException))
                ? types::SeverityType::Warning
                : types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);

        // TODO: Figure out a way to clear data in case of any failure at
        // runtime.

        // set present property to false for any error case. In future this will
        // be replaced by presence logic.
        // Update Present property for this FRU only if we handle Present
        // property for the FRU.
        if (isPresentPropertyHandlingRequired(
                m_parsedJson["frus"][i_vpdFilePath].at(0)))
        {
            setPresentProperty(i_vpdFilePath, false);
        }

        m_semaphore.release();
        return std::make_tuple(false, i_vpdFilePath);
    }
}

bool Worker::skipPathForCollection(const std::string& i_vpdFilePath)
{
    if (i_vpdFilePath.empty())
    {
        return true;
    }

    // skip processing of system VPD again as it has been already collected.
    if (i_vpdFilePath == SYSTEM_VPD_FILE_PATH)
    {
        return true;
    }

    if (dbusUtility::isChassisPowerOn())
    {
        // If chassis is powered on, skip collecting FRUs which are
        // powerOffOnly.

        uint16_t l_errCode = 0;
        if (jsonUtility::isFruPowerOffOnly(m_parsedJson, i_vpdFilePath,
                                           l_errCode))
        {
            return true;
        }
        else if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to check if FRU is power off only for FRU [" +
                    i_vpdFilePath +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode),
                PlaceHolder::COLLECTION);
        }

        std::string l_invPath = jsonUtility::getInventoryObjPathFromJson(
            m_parsedJson, i_vpdFilePath, l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to get inventory path from JSON for FRU [" +
                    i_vpdFilePath +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode),
                PlaceHolder::COLLECTION);

            return false;
        }

        const std::string& l_invPathLeafValue =
            sdbusplus::message::object_path(l_invPath).filename();

        if ((l_invPathLeafValue.find("pcie_card", 0) != std::string::npos))
        {
            return true;
        }
    }

    return false;
}

void Worker::collectFrusFromJson()
{
    // A parsed JSON file should be present to pick FRUs EEPROM paths
    if (m_parsedJson.empty())
    {
        throw JsonException(
            std::string(__FUNCTION__) +
                ": Config JSON is mandatory for processing of FRUs through this API.",
            m_configJsonPath);
    }

    const nlohmann::json& listOfFrus =
        m_parsedJson["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& itemFRUS : listOfFrus.items())
    {
        const std::string& vpdFilePath = itemFRUS.key();

        if (skipPathForCollection(vpdFilePath))
        {
            continue;
        }

        try
        {
            std::thread{[vpdFilePath, this]() {
                const auto& l_parseResult = parseAndPublishVPD(vpdFilePath);

                m_mutex.lock();
                m_activeCollectionThreadCount--;
                m_mutex.unlock();

                if (!m_activeCollectionThreadCount)
                {
                    m_isAllFruCollected = true;
                }
            }}.detach();
        }
        catch (const std::exception& l_ex)
        {
            // add vpdFilePath(EEPROM path) to failed list
            m_failedEepromPaths.push_front(vpdFilePath);
        }
    }
}

void Worker::deleteFruVpd(const std::string& i_dbusObjPath)
{
    if (i_dbusObjPath.empty())
    {
        throw std::runtime_error("Given DBus object path is empty.");
    }

    uint16_t l_errCode = 0;
    const std::string& l_fruPath =
        jsonUtility::getFruPathFromJson(m_parsedJson, i_dbusObjPath, l_errCode);

    if (l_errCode)
    {
        logging::logMessage(
            "Failed to get FRU path for inventory path [" + i_dbusObjPath +
            "], error : " + commonUtility::getErrCodeMsg(l_errCode) +
            " Aborting FRU VPD deletion.");
        return;
    }

    try
    {
        if (jsonUtility::isActionRequired(m_parsedJson, l_fruPath, "preAction",
                                          "deletion", l_errCode))
        {
            if (!processPreAction(l_fruPath, "deletion", l_errCode))
            {
                std::string l_msg = "Pre action failed";
                if (l_errCode)
                {
                    l_msg += " Reason: " +
                             commonUtility::getErrCodeMsg(l_errCode);
                }
                throw std::runtime_error(l_msg);
            }
        }
        else if (l_errCode)
        {
            logging::logMessage(
                "Failed to check if pre action required for FRU [" + l_fruPath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        vpdSpecificUtility::resetObjTreeVpd(l_fruPath, m_parsedJson, l_errCode);

        if (l_errCode)
        {
            throw std::runtime_error(
                "Failed to clear data under PIM for FRU [" + l_fruPath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        if (jsonUtility::isActionRequired(m_parsedJson, l_fruPath, "postAction",
                                          "deletion", l_errCode))
        {
            if (!processPostAction(l_fruPath, "deletion"))
            {
                throw std::runtime_error("Post action failed");
            }
        }
        else if (l_errCode)
        {
            logging::logMessage(
                "Failed to check if post action required during deletion for FRU [" +
                l_fruPath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        logging::logMessage(
            "Successfully completed deletion of FRU VPD for " + i_dbusObjPath);
    }
    catch (const std::exception& l_ex)
    {
        uint16_t l_errCode = 0;
        std::string l_errMsg =
            "Failed to delete VPD for FRU : " + i_dbusObjPath +
            " error: " + std::string(l_ex.what());

        if (jsonUtility::isActionRequired(m_parsedJson, l_fruPath,
                                          "postFailAction", "deletion",
                                          l_errCode))
        {
            if (!jsonUtility::executePostFailAction(m_parsedJson, l_fruPath,
                                                    "deletion", l_errCode))
            {
                l_errMsg += ". Post fail action also failed, error : " +
                            commonUtility::getErrCodeMsg(l_errCode);
            }
        }
        else if (l_errCode)
        {
            l_errMsg +=
                ". Failed to check if post fail action required, error : " +
                commonUtility::getErrCodeMsg(l_errCode);
        }

        logging::logMessage(l_errMsg);
    }
}

void Worker::setPresentProperty(const std::string& i_vpdPath,
                                const bool& i_value)
{
    try
    {
        if (i_vpdPath.empty())
        {
            throw std::runtime_error(
                "Path is empty. Can't set present property");
        }

        types::ObjectMap l_objectInterfaceMap;

        // If the given path is EEPROM path.
        if (m_parsedJson["frus"].contains(i_vpdPath))
        {
            for (const auto& l_Fru : m_parsedJson["frus"][i_vpdPath])
            {
                sdbusplus::message::object_path l_fruObjectPath(
                    l_Fru["inventoryPath"]);

                types::PropertyMap l_propertyValueMap;
                l_propertyValueMap.emplace("Present", i_value);

                uint16_t l_errCode = 0;
                types::InterfaceMap l_interfaces;
                vpdSpecificUtility::insertOrMerge(
                    l_interfaces, constants::inventoryItemInf,
                    move(l_propertyValueMap), l_errCode);

                if (l_errCode)
                {
                    logging::logMessage(
                        "Failed to insert value into map, error : " +
                        commonUtility::getErrCodeMsg(l_errCode));
                }

                l_objectInterfaceMap.emplace(std::move(l_fruObjectPath),
                                             std::move(l_interfaces));
            }
        }
        else
        {
            // consider it as an inventory path.
            if (i_vpdPath.find(constants::pimPath) != constants::VALUE_0)
            {
                throw std::runtime_error(
                    "Invalid inventory path: " + i_vpdPath);
            }

            types::PropertyMap l_propertyValueMap;
            l_propertyValueMap.emplace("Present", i_value);

            uint16_t l_errCode = 0;
            types::InterfaceMap l_interfaces;
            vpdSpecificUtility::insertOrMerge(
                l_interfaces, constants::inventoryItemInf,
                move(l_propertyValueMap), l_errCode);

            if (l_errCode)
            {
                logging::logMessage(
                    "Failed to insert value into map, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }

            l_objectInterfaceMap.emplace(i_vpdPath, std::move(l_interfaces));
        }

        // Notify PIM
        if (!dbusUtility::callPIM(move(l_objectInterfaceMap)))
        {
            throw DbusException(
                std::string(__FUNCTION__) +
                "Call to PIM failed while setting present property for path " +
                i_vpdPath);
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Warning,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Worker::performVpdRecollection()
{
    try
    {
        // Check if system config JSON is present
        if (m_parsedJson.empty())
        {
            throw std::runtime_error(
                "System config json object is empty, can't process recollection.");
        }

        uint16_t l_errCode = 0;
        const auto& l_frusReplaceableAtStandby =
            jsonUtility::getListOfFrusReplaceableAtStandby(m_parsedJson,
                                                           l_errCode);

        if (l_errCode)
        {
            logging::logMessage(
                "Failed to get list of FRUs replaceable at runtime, error : " +
                commonUtility::getErrCodeMsg(l_errCode));
            return;
        }

        for (const auto& l_fruInventoryPath : l_frusReplaceableAtStandby)
        {
            // ToDo: Add some logic/trace to know the flow to
            // collectSingleFruVpd has been directed via
            // performVpdRecollection.
            collectSingleFruVpd(l_fruInventoryPath);
        }
        return;
    }

    catch (const std::exception& l_ex)
    {
        // TODO Log PEL
        logging::logMessage(
            "VPD recollection failed with error: " + std::string(l_ex.what()));
    }
}

void Worker::collectSingleFruVpd(
    const sdbusplus::message::object_path& i_dbusObjPath)
{
    std::string l_fruPath{};
    uint16_t l_errCode = 0;

    try
    {
        // Check if system config JSON is present
        if (m_parsedJson.empty())
        {
            logging::logMessage(
                "System config JSON object not present. Single FRU VPD collection is not performed for " +
                std::string(i_dbusObjPath));
            return;
        }

        // Get FRU path for the given D-bus object path from JSON
        l_fruPath = jsonUtility::getFruPathFromJson(m_parsedJson, i_dbusObjPath,
                                                    l_errCode);

        if (l_fruPath.empty())
        {
            if (l_errCode)
            {
                logging::logMessage(
                    "Failed to get FRU path for [" +
                    std::string(i_dbusObjPath) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode) +
                    " Aborting single FRU VPD collection.");
                return;
            }

            logging::logMessage(
                "D-bus object path not present in JSON. Single FRU VPD collection is not performed for " +
                std::string(i_dbusObjPath));
            return;
        }

        // Check if host is up and running
        if (dbusUtility::isHostRunning())
        {
            uint16_t l_errCode = 0;
            bool isFruReplaceableAtRuntime =
                jsonUtility::isFruReplaceableAtRuntime(m_parsedJson, l_fruPath,
                                                       l_errCode);

            if (l_errCode)
            {
                logging::logMessage(
                    "Failed to check if FRU is replaceable at runtime for FRU : [" +
                    std::string(i_dbusObjPath) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
                return;
            }

            if (!isFruReplaceableAtRuntime)
            {
                logging::logMessage(
                    "Given FRU is not replaceable at host runtime. Single FRU VPD collection is not performed for " +
                    std::string(i_dbusObjPath));
                return;
            }
        }
        else if (dbusUtility::isBMCReady())
        {
            uint16_t l_errCode = 0;
            bool isFruReplaceableAtStandby =
                jsonUtility::isFruReplaceableAtStandby(m_parsedJson, l_fruPath,
                                                       l_errCode);

            if (l_errCode)
            {
                logging::logMessage(
                    "Error while checking if FRU is replaceable at standby for FRU [" +
                    std::string(i_dbusObjPath) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }

            bool isFruReplaceableAtRuntime =
                jsonUtility::isFruReplaceableAtRuntime(m_parsedJson, l_fruPath,
                                                       l_errCode);

            if (l_errCode)
            {
                logging::logMessage(
                    "Failed to check if FRU is replaceable at runtime for FRU : [" +
                    std::string(i_dbusObjPath) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
                return;
            }

            if (!isFruReplaceableAtStandby && (!isFruReplaceableAtRuntime))
            {
                logging::logMessage(
                    "Given FRU is neither replaceable at standby nor replaceable
                    "at runtime. Single FRU VPD collection is not performed for " +
                    std::string(i_dbusObjPath));
                return;
            }
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            l_fruPath, types::VpdCollectionStatus::InProgress, m_parsedJson,
            l_errCode);
        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status for path " + l_fruPath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        // Parse VPD
        types::VPDMapVariant l_parsedVpd = parseVpdFile(l_fruPath);

        // If l_parsedVpd is pointing to std::monostate
        if (l_parsedVpd.index() == 0)
        {
            // As empty parsedVpdMap recieved for some reason, but still
            // considered VPD collection is completed. Hence FRU collection
            // Status will be set as completed.
            m_logger->logMessage("Empty parsed VPD map received for " +
                                 std::string(i_dbusObjPath));

            // Stale data from the previous boot can be present on the system.
            // so clearing of data.
            vpdSpecificUtility::resetObjTreeVpd(std::string(i_dbusObjPath),
                                                m_parsedJson, l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to reset data under PIM for path [" +
                    std::string(i_dbusObjPath) +
                    "] error : " + commonUtility::getErrCodeMsg(l_errCode));
            }
        }
        else
        {
            types::ObjectMap l_dbusObjectMap;
            // Get D-bus object map from worker class
            populateDbus(l_parsedVpd, l_dbusObjectMap, l_fruPath);

            if (l_dbusObjectMap.empty())
            {
                throw std::runtime_error(
                    "Failed to create D-bus object map. Single FRU VPD collection failed for " +
                    std::string(i_dbusObjPath));
            }

            // Call method to update the dbus
            if (!dbusUtility::publishVpdOnDBus(move(l_dbusObjectMap)))
            {
                throw std::runtime_error(
                    "Notify PIM failed. Single FRU VPD collection failed for " +
                    std::string(i_dbusObjPath));
            }
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            l_fruPath, types::VpdCollectionStatus::Completed, m_parsedJson,
            l_errCode);
        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status as completed for path " +
                l_fruPath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }
    }
    catch (const std::exception& l_error)
    {
        std::string l_errMsg;
        vpdSpecificUtility::resetObjTreeVpd(std::string(i_dbusObjPath),
                                            m_parsedJson, l_errCode);

        if (l_errCode)
        {
            l_errMsg += "Failed to reset data under PIM for path [" +
                        std::string(i_dbusObjPath) + "], error : " +
                        commonUtility::getErrCodeMsg(l_errCode) + ". ";
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            l_fruPath, types::VpdCollectionStatus::Failed, m_parsedJson,
            l_errCode);
        if (l_errCode)
        {
            l_errMsg += "Failed to set collection status as failed for path " +
                        l_fruPath +
                        "Reason: " + commonUtility::getErrCodeMsg(l_errCode);
        }
        // TODO: Log PEL
        m_logger->logMessage(l_errMsg + std::string(l_error.what()));
    }
}

void Worker::checkAndExecutePostFailAction(
    const std::string& i_vpdFilePath,
    const std::string& i_flowFlag) const noexcept
{
    try
    {
        uint16_t l_errCode{0};
        if (!jsonUtility::isActionRequired(m_parsedJson, i_vpdFilePath,
                                           "postFailAction", i_flowFlag,
                                           l_errCode))
        {
            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to check if postFailAction is required. Error: " +
                        commonUtility::getErrCodeMsg(l_errCode),
                    i_flowFlag == "collection" ? PlaceHolder::COLLECTION
                                               : PlaceHolder::DEFAULT);
            }
            return;
        }

        if (!jsonUtility::executePostFailAction(m_parsedJson, i_vpdFilePath,
                                                i_flowFlag, l_errCode))
        {
            m_logger->logMessage("Failed to execute postFailAction. Error: " +
                                     commonUtility::getErrCodeMsg(l_errCode),
                                 i_flowFlag == "collection"
                                     ? PlaceHolder::COLLECTION
                                     : PlaceHolder::DEFAULT);
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to check and execute postFailAction. Error: " +
                std::string(l_ex.what()),
            i_flowFlag == "collection" ? PlaceHolder::COLLECTION
                                       : PlaceHolder::DEFAULT);
    }
}

} // namespace vpd
