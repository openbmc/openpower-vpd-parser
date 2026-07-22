#include "config.h"

#include "worker.hpp"

#include "backup_restore.hpp"
#include "constants.hpp"
#include "error_codes.hpp"
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
#include <format>
#include <fstream>
#include <future>
#include <typeindex>
#include <unordered_set>

namespace vpd
{

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
                m_logger->logMessage(
                    "Unknown Keyword =" + kwd + " found in keyword VPD map");
                continue;
            }
        }
        else
        {
            m_logger->logMessage(
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
                m_logger->logMessage(
                    "Failed to insert value into map, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }
        }
    }
}

void Worker::populateInterfaces(const nlohmann::json& interfaceJson,
                                types::InterfaceMap& interfaceMap,
                                const types::VPDMapVariant& parsedVpdMap,
                                const std::string& i_inventoryPath)
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
                if (property.compare("LocationCode") == 0)
                {
                    const auto l_expandedLcResult =
                        vpdSpecificUtility::getExpandedLocationCode(
                            i_inventoryPath,
                            propValuePair.value().get<std::string>(),
                            parsedVpdMap);

                    if (!l_expandedLcResult.has_value())
                    {
                        m_logger->logMessage(
                            "Failed to get expanded location code for location code - " +
                            propValuePair.value().get<std::string>() +
                            " ,error : " +
                            commonUtility::getErrCodeMsg(
                                l_expandedLcResult.error()));
                    }

                    propertyMap.emplace(
                        property,
                        l_expandedLcResult.value_or(
                            propValuePair.value().get<std::string>()));
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
                            m_logger->logMessage(
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
                                m_logger->logMessage(
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
                                m_logger->logMessage(
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
                            m_logger->logMessage(
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
            m_logger->logMessage("Failed to insert value into map, error : " +
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
            m_logger->logMessage("Failed to insert value into map, error : " +
                                 commonUtility::getErrCodeMsg(l_errCode));
        }
    }
}

void Worker::processExtraInterfaces(const nlohmann::json& singleFru,
                                    types::InterfaceMap& interfaces,
                                    const types::VPDMapVariant& parsedVpdMap,
                                    const std::string& i_inventoryPath)
{
    populateInterfaces(singleFru["extraInterfaces"], interfaces, parsedVpdMap,
                       i_inventoryPath);

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

void Worker::processInheritFlag(const nlohmann::json& i_configJson,
                                const types::VPDMapVariant& parsedVpdMap,
                                types::InterfaceMap& interfaces,
                                const std::string& i_inventoryPath)
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

    if (i_configJson.contains("commonInterfaces"))
    {
        populateInterfaces(i_configJson["commonInterfaces"], interfaces,
                           parsedVpdMap, i_inventoryPath);
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
            m_logger->logMessage("Failed to get CCIN kwd value, error : " +
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
            m_logger->logMessage(
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
            m_logger->logMessage(
                "Failed to insert interface into map, error : " +
                commonUtility::getErrCodeMsg(l_errCode));
        }
    }

    // if chassis is power on. Enabled property should be there on D-Bus.
    // Don't process.
    return;
}

void Worker::populateDbus(const nlohmann::json& i_configJsonObj,
                          const types::VPDMapVariant& parsedVpdMap,
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
    if (!i_configJsonObj.empty())
    {
        types::InterfaceMap interfaces;

        for (const auto& aFru : i_configJsonObj["frus"][vpdFilePath])
        {
            const auto& inventoryPath = aFru["inventoryPath"];
            sdbusplus::object_path fruObjectPath(inventoryPath);
            if (aFru.contains("ccin"))
            {
                if (!processFruWithCCIN(aFru, parsedVpdMap))
                {
                    continue;
                }
            }

            if (aFru.value("inherit", true))
            {
                processInheritFlag(i_configJsonObj, parsedVpdMap, interfaces,
                                   inventoryPath);
            }
            else if (aFru.contains("copyRecords"))
            {
                // specific record needs to be copied.
                processCopyRecordFlag(aFru, parsedVpdMap, interfaces);
            }
            else if (aFru.contains("skipRecords"))
            {
                // specific record needs to be skipped and copy rest records.
                processSkipRecordsFlag(aFru, parsedVpdMap, interfaces);
            }

            // Process commonInterfaces for FRUs where inheritCI=true but
            // inherit=false
            if (!aFru.value("inherit", true) &&
                aFru.value("inheritCI", false) &&
                i_configJsonObj.contains("commonInterfaces"))
            {
                populateInterfaces(i_configJsonObj["commonInterfaces"],
                                   interfaces, parsedVpdMap, inventoryPath);
            }

            if (aFru.contains("extraInterfaces"))
            {
                // Process extra interfaces w.r.t a FRU.
                processExtraInterfaces(aFru, interfaces, parsedVpdMap,
                                       inventoryPath);
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

types::BaseActionResult Worker::processPreAction(
    const nlohmann::json& i_configJson, const std::string& i_vpdFilePath,
    const std::string& i_flagToProcess, uint16_t& o_errCode)
{
    o_errCode = 0;
    if (i_vpdFilePath.empty() || i_flagToProcess.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return types::BaseActionResult{};
    }

    const types::BaseActionResult l_result = jsonUtility::executeBaseAction(
        i_configJson, "preAction", i_vpdFilePath, i_flagToProcess, o_errCode);

    if (!l_result.m_success &&
        (i_flagToProcess.compare("collection") == constants::STR_CMP_SUCCESS))
    {
        // TODO: Need a way to delete inventory object from Dbus and persisted
        // data section in case any FRU is not present or there is any
        // problem in collecting it. Once it has been deleted, it can be
        // re-created in the flow of priming the inventory. This needs to be
        // done either here or in the exception section of "parseAndPublishVPD"
        // API. Any failure in the process of collecting FRU will land up in the
        // exception of "parseAndPublishVPD".

        // If the FRU is not there, clear the VINI/CCIN data.
        // Entity manager probes for this keyword to look for this
        // FRU, now if the data is persistent on BMC and FRU is
        // removed this can lead to ambiguity. Hence clearing this
        // Keyword if FRU is absent.
        const auto& inventoryPath =
            i_configJson["frus"][i_vpdFilePath].at(0).value("inventoryPath",
                                                            "");

        if (!inventoryPath.empty())
        {
            types::ObjectMap l_pimObjMap{
                {inventoryPath,
                 {{constants::kwdVpdInf,
                   {{constants::kwdCCIN, types::BinaryVector{}}}}}}};

            // Call dbus method to update on dbus
            if (!dbusUtility::publishVpdOnDBus(std::move(l_pimObjMap)))
            {
                m_logger->logMessage(
                    "Call to PIM failed for file " + i_vpdFilePath);
            }
        }
        else
        {
            m_logger->logMessage(
                "Inventory path is empty in Json for file " + i_vpdFilePath);
        }
    }
    return l_result;
}

bool Worker::processPostAction(
    const nlohmann::json& i_configJson, const std::string& i_vpdFruPath,
    const std::string& i_flagToProcess,
    const std::optional<types::VPDMapVariant> i_parsedVpd)
{
    if (i_vpdFruPath.empty() || i_flagToProcess.empty())
    {
        m_logger->logMessage(
            "Invalid input parameter. Abort processing post action");
        return false;
    }

    if (!i_parsedVpd.has_value())
    {
        m_logger->logMessage("Empty VPD Map");
        return false;
    }

    // Check if post action tag is to be triggered in the flow of collection
    // based on some CCIN value?
    uint16_t l_errCode = 0;

    if (i_configJson["frus"][i_vpdFruPath]
            .at(0)["postAction"][i_flagToProcess]
            .contains("ccin"))
    {
        // CCIN match is required to process post action for this FRU as it
        // contains the flag.
        if (!vpdSpecificUtility::findCcinInVpd(
                i_configJson["frus"][i_vpdFruPath].at(
                    0)["postAction"][i_flagToProcess],
                i_parsedVpd.value(), l_errCode))
        {
            if (l_errCode)
            {
                // ToDo - Check if PEL is required in case of RECORD_NOT_FOUND
                // and KEYWORD_NOT_FOUND error codes.
                m_logger->logMessage("Failed to find CCIN in VPD, error : " +
                                     commonUtility::getErrCodeMsg(l_errCode));
            }

            // If CCIN is not found, implies post action processing is not
            // required for this FRU. Let the flow continue.
            return true;
        }
    }

    types::BaseActionResult l_actionResult = jsonUtility::executeBaseAction(
        i_configJson, "postAction", i_vpdFruPath, i_flagToProcess, l_errCode);
    // Handle post action execution failure
    if (!l_actionResult.m_success)
    {
        m_logger->logMessage(std::format(
            "processPostAction: Execution failed for FRU [{}]. "
            "Failed tag: '{}', Reason: {}",
            i_vpdFruPath,
            l_actionResult.m_failedTag.empty() ? "unknown"
                                               : l_actionResult.m_failedTag,
            commonUtility::getErrCodeMsg(l_actionResult.m_failedTagErrorCode)));
        return false;
    }

    return true;
}

types::VPDMapVariant Worker::parseVpdFile(
    const nlohmann::json& i_configJsonObj, const std::string& i_vpdFilePath,
    const bool& i_processRedundant, bool& o_presenceState)
{
    o_presenceState = false;
    try
    {
        uint16_t l_errCode = 0;

        if (i_vpdFilePath.empty())
        {
            throw FirmwareException(
                " Empty VPD file path passed. Abort parseVpdFile");
        }

        bool isPreActionRequired = false;
        if (!i_configJsonObj.empty())
        {
            if (jsonUtility::isActionRequired(i_configJsonObj, i_vpdFilePath,
                                              "preAction", "collection",
                                              l_errCode))
            {
                isPreActionRequired = true;
                const types::BaseActionResult l_actionResult = processPreAction(
                    i_configJsonObj, i_vpdFilePath, "collection", l_errCode);

                if (l_actionResult.m_gpioPresenceErrorCode ==
                    error_code::DEVICE_NOT_PRESENT)
                {
                    m_logger->logMessage(
                        commonUtility::getErrCodeMsg(
                            l_actionResult.m_gpioPresenceErrorCode) +
                            i_vpdFilePath,
                        PlaceHolder::COLLECTION);

                    // since pre action is reporting device not present,
                    // execute post fail action
                    checkAndExecutePostFailAction(i_configJsonObj,
                                                  i_vpdFilePath, "collection");

                    // Presence pin has been read successfully and has been
                    // read as false, so this is not a failure case, hence
                    // returning empty variant so that pre action is not
                    // marked as failed.
                    // o_presenceState already false from init.
                    return types::VPDMapVariant{};
                }

                if (l_actionResult.m_presenceStatus ==
                        types::PresenceStatus::UNKNOWN ||
                    l_actionResult.m_presenceStatus ==
                        types::PresenceStatus::NOT_APPLICABLE)
                {
                    o_presenceState = std::filesystem::exists(i_vpdFilePath);
                }
                else if (l_actionResult.m_presenceStatus ==
                         types::PresenceStatus::PRESENT)
                {
                    o_presenceState = true;
                }

                if (!l_actionResult.m_success)
                {
                    // One of the preAction tags failed; we can't collect VPD.
                    throw FirmwareException(std::format(
                        " Pre-Action failed with error: {}. Aborting parsing of VPD file {}.",
                        commonUtility::getErrCodeMsg(l_errCode),
                        i_vpdFilePath));
                }
            }
            else if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to check if pre action required for FRU [" +
                    i_vpdFilePath +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }

            // Skip VPD collection if VPD path is redundant path and
            // i_processRedundant is set to false.
            if (i_configJsonObj["frus"][i_vpdFilePath].at(0).value(
                    "isRedundant", false) &&
                !i_processRedundant)
            {
                return types::VPDMapVariant{};
            }
        }

        // If no pre-action was required, determine presence from file
        // existence.
        if (!isPreActionRequired)
        {
            o_presenceState = std::filesystem::exists(i_vpdFilePath);
        }

        if (!std::filesystem::exists(i_vpdFilePath))
        {
            if (isPreActionRequired)
            {
                throw EepromException(std::format(
                    " Could not find EEPROM: {} after preAction. Abort parsing of VPD file.",
                    i_vpdFilePath));
            }
            return types::VPDMapVariant{};
        }

        std::shared_ptr<Parser> vpdParser =
            std::make_shared<Parser>(i_vpdFilePath, i_configJsonObj);

        types::VPDMapVariant l_parsedVpd = vpdParser->parse();

        // Before returning, as collection is over, check if FRU qualifies for
        // any post action in the flow of collection.
        // Note: Don't change the order, post action needs to be processed only
        // after collection for FRU is successfully done.
        if (jsonUtility::isActionRequired(i_configJsonObj, i_vpdFilePath,
                                          "postAction", "collection",
                                          l_errCode))
        {
            if (!processPostAction(i_configJsonObj, i_vpdFilePath, "collection",
                                   l_parsedVpd))
            {
                // Post action was required but failed while executing.
                // Behaviour can be undefined.

                m_logger->logMessage(
                    std::string("Required post action failed for path [" +
                                i_vpdFilePath + "]."),
                    PlaceHolder::PEL,
                    types::PelInfoTuple{types::ErrorType::InternalFailure,
                                        types::SeverityType::Warning, 0,
                                        std::nullopt, std::nullopt,
                                        std::nullopt, std::nullopt,
                                        std::nullopt});
            }
        }
        else if (l_errCode)
        {
            m_logger->logMessage(
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
        checkAndExecutePostFailAction(i_configJsonObj, i_vpdFilePath,
                                      "collection");

        if (typeid(l_ex) == typeid(DataException))
        {
            throw DataException(l_exMsg);
        }
        else if (typeid(l_ex) == typeid(EccException))
        {
            throw EccException(l_exMsg);
        }

        // Throw rest of the error as it is.
        throw;
    }
}

std::tuple<bool, bool> Worker::parseAndPublishVPD(
    const nlohmann::json& i_configJson, const std::string& i_vpdFilePath,
    const bool& i_processRedundant)
{
    uint16_t l_errCode = 0;
    bool l_presenceState = false;
    try
    {
        if (i_vpdFilePath.empty())
        {
            m_logger->logMessage(
                "Empty VPD file path received, aborting parse and publish VPD.",
                PlaceHolder::COLLECTION);
            return std::make_tuple(false, l_presenceState);
        }

        // When `i_processRedundant` is false, skip D-Bus updates for
        // redundant FRUs and only perform pre-action, if any.
        if (i_configJson["frus"][i_vpdFilePath].at(0).value("isRedundant",
                                                            false) &&
            !i_processRedundant)
        {
            const bool l_status =
                processRedundantPreAction(i_configJson, i_vpdFilePath);

            return std::make_tuple(l_status, l_presenceState);
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            i_vpdFilePath, types::VpdCollectionStatus::InProgress, i_configJson,
            l_errCode);
        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status for path " + i_vpdFilePath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        const types::VPDMapVariant& parsedVpdMap = parseVpdFile(
            i_configJson, i_vpdFilePath, i_processRedundant, l_presenceState);

        if (isPresentPropertyHandlingRequired(
                i_configJson["frus"][i_vpdFilePath].at(0)))
        {
            setPresentProperty(i_configJson, i_vpdFilePath, l_presenceState);
        }

        if (!std::holds_alternative<std::monostate>(parsedVpdMap))
        {
            types::ObjectMap objectInterfaceMap;
            populateDbus(i_configJson, parsedVpdMap, objectInterfaceMap,
                         i_vpdFilePath);

            // Call dbus method to update on dbus
            if (!dbusUtility::publishVpdOnDBus(move(objectInterfaceMap)))
            {
                throw FirmwareException(std::format(
                    "Call to publish on VPD on Dbus failed for EEPROM {}.",
                    i_vpdFilePath));
            }
        }
        else
        {
            // Stale data from the previous boot can be present on the system.
            // so clearing of data in case of empty map received.
            // As empty parsedVpdMap received for some reason, but still
            // considered VPD collection is completed. Hence FRU collection
            // Status will be set as completed.

            vpdSpecificUtility::resetObjTreeVpd(i_vpdFilePath, i_configJson,
                                                l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to reset data under PIM for path [" +
                    i_vpdFilePath +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }

            m_logger->logMessage("Empty parsedVpdMap received for path [" +
                                     i_vpdFilePath + "]. Check PEL for reason.",
                                 PlaceHolder::COLLECTION);
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            i_vpdFilePath, types::VpdCollectionStatus::Completed, i_configJson,
            l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status as completed for path " +
                i_vpdFilePath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        return std::make_tuple(true, l_presenceState);
    }
    catch (const std::exception& l_ex)
    {
        uint16_t l_errCode = 0;

        // stale data can be present on the system from previous boot. so
        // clearing of data in case of failure.
        vpdSpecificUtility::resetObjTreeVpd(i_vpdFilePath, i_configJson,
                                            l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to reset under PIM for path [" + i_vpdFilePath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            i_vpdFilePath, types::VpdCollectionStatus::Failed, i_configJson,
            l_errCode);
        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status as failed for path " +
                i_vpdFilePath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        // handle all the exceptions internally. Return collection status and
        // presence state.
        if (typeid(l_ex) == std::type_index(typeid(DataException)))
        {
            // In case of pass1 planar, VPD can be corrupted on PCIe cards. Skip
            // logging error for these cases.
            if (vpdSpecificUtility::isPass1Planar(l_errCode))
            {
                std::string l_invPath =
                    jsonUtility::getInventoryObjPathFromJson(i_vpdFilePath,
                                                             l_errCode);

                if (l_errCode != 0)
                {
                    m_logger->logMessage(
                        "Failed to get inventory object path from JSON for FRU [" +
                            i_vpdFilePath + "], error: " +
                            commonUtility::getErrCodeMsg(l_errCode),
                        PlaceHolder::COLLECTION);
                }

                const std::string& l_invPathLeafValue =
                    sdbusplus::object_path(l_invPath).filename();

                if ((l_invPathLeafValue.find("pcie_card", 0) !=
                     std::string::npos))
                {
                    // skip logging any PEL for PCIe cards on pass 1 planar.
                    return std::make_tuple(false, l_presenceState);
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

        if (typeid(l_ex) == std::type_index(typeid(FirmwareException)) ||
            typeid(l_ex) == std::type_index(typeid(EepromException)))
        {
            m_logger->logMessage(l_ex.what(), PlaceHolder::COLLECTION);
        }
        else
        {
            // Log PEL only processing was ok but data/ECC had some issue or
            // some runtime exception took place which is not normal.
            // Commenting Async PELs for the time being, till we handle presence
            // locally.
            m_logger->logMessage(std::format(
                "ParseAndPublish VPD failed. Reason: {}.", l_ex.what()));
            /* m_logger->logMessage(
                 std::string("ParseAndPublish VPD failed for [reason] ") +
                     EventLogger::getErrorMsg(l_ex),
                 PlaceHolder::ASYNC_PEL,
                 types::PelInfoTuple{
                     EventLogger::getErrorType(l_ex),
                     (typeid(l_ex) == typeid(DataException)) ||
                             (typeid(l_ex) == typeid(EccException))
                         ? types::SeverityType::Warning
                         : types::SeverityType::Informational,
                     0, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                     std::nullopt}); */
        }

        return std::make_tuple(false, l_presenceState);
    }
}

bool Worker::skipPathForCollection(const nlohmann::json& i_configJson,
                                   const std::string& i_vpdFilePath)
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
        if (jsonUtility::isFruPowerOffOnly(i_configJson, i_vpdFilePath,
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

        std::string l_invPath =
            jsonUtility::getInventoryObjPathFromJson(i_vpdFilePath, l_errCode);

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
            sdbusplus::object_path(l_invPath).filename();

        if ((l_invPathLeafValue.find("pcie_card", 0) != std::string::npos))
        {
            return true;
        }
    }

    return false;
}

void Worker::deleteFruVpd(const nlohmann::json& i_configJsonObj,
                          const std::string& i_dbusObjPath)
{
    if (i_configJsonObj.empty())
    {
        throw std::runtime_error("Empty configuration JSON provided");
    }

    if (i_dbusObjPath.empty())
    {
        throw std::runtime_error("Given DBus object path is empty.");
    }

    uint16_t l_errCode = 0;

    const std::string& l_fruPath =
        jsonUtility::getFruPathFromJson(i_dbusObjPath, l_errCode);

    if (l_errCode)
    {
        m_logger->logMessage(
            "Failed to get FRU path for inventory path [" + i_dbusObjPath +
            "], error : " + commonUtility::getErrCodeMsg(l_errCode) +
            " Aborting FRU VPD deletion.");
        return;
    }

    try
    {
        if (jsonUtility::isActionRequired(i_configJsonObj, l_fruPath,
                                          "preAction", "deletion", l_errCode))
        {
            const types::BaseActionResult l_preActResult = processPreAction(
                i_configJsonObj, l_fruPath, "deletion", l_errCode);
            if (!l_preActResult.m_success)
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
            m_logger->logMessage(
                "Failed to check if pre action required for FRU [" + l_fruPath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        vpdSpecificUtility::resetObjTreeVpd(l_fruPath, i_configJsonObj,
                                            l_errCode);

        if (l_errCode)
        {
            throw std::runtime_error(
                "Failed to clear data under PIM for FRU [" + l_fruPath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        if (jsonUtility::isActionRequired(i_configJsonObj, l_fruPath,
                                          "postAction", "deletion", l_errCode))
        {
            if (!processPostAction(i_configJsonObj, l_fruPath, "deletion"))
            {
                throw std::runtime_error("Post action failed");
            }
        }
        else if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to check if post action required during deletion for FRU [" +
                l_fruPath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        m_logger->logMessage(
            "Successfully completed deletion of FRU VPD for " + i_dbusObjPath);
    }
    catch (const std::exception& l_ex)
    {
        uint16_t l_errCode = 0;
        std::string l_errMsg =
            "Failed to delete VPD for FRU : " + i_dbusObjPath +
            " error: " + std::string(l_ex.what());

        if (jsonUtility::isActionRequired(i_configJsonObj, l_fruPath,
                                          "postFailAction", "deletion",
                                          l_errCode))
        {
            if (!jsonUtility::executePostFailAction(i_configJsonObj, l_fruPath,
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

        m_logger->logMessage(l_errMsg);
    }
}

void Worker::setPresentProperty(const nlohmann::json& i_configJson,
                                const std::string& i_vpdPath,
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

        types::PropertyMap l_propertyValueMap;
        l_propertyValueMap.emplace("Present", i_value);

        uint16_t l_errCode = 0;
        types::InterfaceMap l_interfaces;
        vpdSpecificUtility::insertOrMerge(l_interfaces,
                                          constants::inventoryItemInf,
                                          move(l_propertyValueMap), l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage("Failed to insert value into map, error : " +
                                 commonUtility::getErrCodeMsg(l_errCode));
        }

        // If the given path is EEPROM path.
        if (i_configJson["frus"].contains(i_vpdPath))
        {
            for (const auto& l_Fru : i_configJson["frus"][i_vpdPath])
            {
                sdbusplus::object_path l_fruObjectPath(l_Fru["inventoryPath"]);

                l_objectInterfaceMap.emplace(std::move(l_fruObjectPath),
                                             l_interfaces);
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
            l_objectInterfaceMap.emplace(i_vpdPath, std::move(l_interfaces));
        }

        // Call dbus method to update on dbus
        if (!dbusUtility::publishVpdOnDBus(move(l_objectInterfaceMap)))
        {
            throw DbusException(
                std::string(__FUNCTION__) +
                "Call to PIM failed while setting present property for path " +
                i_vpdPath);
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format(
                "Exception while setting the present property for path {}. Error {}.",
                i_vpdPath, EventLogger::getErrorMsg(l_ex)),
            PlaceHolder::COLLECTION);
    }
}

void Worker::performVpdRecollection(
    const nlohmann::json& i_sysCfgJsonObj) noexcept
{
    try
    {
        if (i_sysCfgJsonObj.empty())
        {
            throw std::runtime_error(
                "System config json object is empty, can't process recollection.");
        }

        uint16_t l_errCode = 0;

        const auto& l_frusReplaceableAtStandby =
            jsonUtility::getListOfFrusReplaceableAtStandby(i_sysCfgJsonObj,
                                                           l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to get list of FRUs replaceable at runtime, error : " +
                commonUtility::getErrCodeMsg(l_errCode));
            return;
        }

        for (const auto& l_fruInventoryPath : l_frusReplaceableAtStandby)
        {
            const auto l_fruPath =
                jsonUtility::getFruPathFromJson(l_fruInventoryPath, l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(std::format(
                    "Failed to get EEPROM path for inventory path [{}], error : {}.",
                    l_fruInventoryPath,
                    commonUtility::getErrCodeMsg(l_errCode)));
                continue;
            }

            parseAndPublishVPD(i_sysCfgJsonObj, l_fruPath);
        }
        return;
    }

    catch (const std::exception& l_ex)
    {
        // TODO Log PEL
        m_logger->logMessage(
            "VPD recollection failed with error: " + std::string(l_ex.what()));
    }
}

void Worker::collectSingleFruVpd(const nlohmann::json& i_configJsonObj,
                                 const sdbusplus::object_path& i_dbusObjPath)
{
    std::string l_fruPath{};
    uint16_t l_errCode = 0;

    try
    {
        // Check if system config JSON is present
        if (i_configJsonObj.empty())
        {
            m_logger->logMessage(
                "Empty configuration JSON provided. Single FRU VPD collection is not performed for " +
                std::string(i_dbusObjPath));
            return;
        }

        // Get FRU path for the given D-bus object path from JSON
        l_fruPath = jsonUtility::getFruPathFromJson(i_dbusObjPath, l_errCode);

        if (l_fruPath.empty())
        {
            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to get FRU path for [" +
                    std::string(i_dbusObjPath) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode) +
                    " Aborting single FRU VPD collection.");
                return;
            }

            m_logger->logMessage(
                "D-bus object path not present in JSON. Single FRU VPD collection is not performed for " +
                std::string(i_dbusObjPath));
            return;
        }

        // Check if host is up and running
        if (dbusUtility::isHostRunning())
        {
            uint16_t l_errCode = 0;
            bool isFruReplaceableAtRuntime =
                jsonUtility::isFruReplaceableAtRuntime(i_configJsonObj,
                                                       l_fruPath, l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to check if FRU is replaceable at runtime for FRU : [" +
                    std::string(i_dbusObjPath) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
                return;
            }

            if (!isFruReplaceableAtRuntime)
            {
                m_logger->logMessage(
                    "Given FRU is not replaceable at host runtime. Single FRU VPD collection is not performed for " +
                    std::string(i_dbusObjPath));
                return;
            }
        }
        else if (dbusUtility::isBMCReady())
        {
            uint16_t l_errCode = 0;
            bool isFruReplaceableAtStandby =
                jsonUtility::isFruReplaceableAtStandby(i_configJsonObj,
                                                       l_fruPath, l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(
                    "Error while checking if FRU is replaceable at standby for FRU [" +
                    std::string(i_dbusObjPath) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }

            bool isFruReplaceableAtRuntime =
                jsonUtility::isFruReplaceableAtRuntime(i_configJsonObj,
                                                       l_fruPath, l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to check if FRU is replaceable at runtime for FRU : [" +
                    std::string(i_dbusObjPath) +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
                return;
            }

            if (!isFruReplaceableAtStandby && (!isFruReplaceableAtRuntime))
            {
                m_logger->logMessage(
                    "Given FRU is neither replaceable at standby nor replaceable at runtime. Single FRU VPD collection is not performed for " +
                    std::string(i_dbusObjPath));
                return;
            }
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            l_fruPath, types::VpdCollectionStatus::InProgress, i_configJsonObj,
            l_errCode);
        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status for path " + l_fruPath +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        // Parse VPD
        bool l_fruPresenceState = false;
        types::VPDMapVariant l_parsedVpd =
            parseVpdFile(i_configJsonObj, l_fruPath, false, l_fruPresenceState);

        if (isPresentPropertyHandlingRequired(
                i_configJsonObj["frus"][l_fruPath].at(0)))
        {
            setPresentProperty(i_configJsonObj, l_fruPath, l_fruPresenceState);
        }

        // If l_parsedVpd is pointing to std::monostate
        if (l_parsedVpd.index() == 0)
        {
            // As empty parsedVpdMap received for some reason, but still
            // considered VPD collection is completed. Hence FRU collection
            // Status will be set as completed.
            m_logger->logMessage("Empty parsed VPD map received for " +
                                 std::string(i_dbusObjPath));

            // Stale data from the previous boot can be present on the
            // system. so clearing of data.
            vpdSpecificUtility::resetObjTreeVpd(std::string(i_dbusObjPath),
                                                i_configJsonObj, l_errCode);

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
            populateDbus(i_configJsonObj, l_parsedVpd, l_dbusObjectMap,
                         l_fruPath);

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
                    "publishVpdOnDBus failed. Single FRU VPD collection failed for " +
                    std::string(i_dbusObjPath));
            }
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            l_fruPath, types::VpdCollectionStatus::Completed, i_configJsonObj,
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
                                            i_configJsonObj, l_errCode);

        if (l_errCode)
        {
            l_errMsg += "Failed to reset data under PIM for path [" +
                        std::string(i_dbusObjPath) + "], error : " +
                        commonUtility::getErrCodeMsg(l_errCode) + ". ";
        }

        vpdSpecificUtility::setCollectionStatusProperty(
            l_fruPath, types::VpdCollectionStatus::Failed, i_configJsonObj,
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
    const nlohmann::json& i_configJson, const std::string& i_vpdFilePath,
    const std::string& i_flowFlag) const noexcept
{
    try
    {
        uint16_t l_errCode{0};
        if (!jsonUtility::isActionRequired(i_configJson, i_vpdFilePath,
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

        if (!jsonUtility::executePostFailAction(i_configJson, i_vpdFilePath,
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

void Worker::processSkipRecordsFlag(const nlohmann::json& i_fruJson,
                                    const types::VPDMapVariant& i_parsedVpdMap,
                                    types::InterfaceMap& o_interfaces)
{
    if (const auto l_ipzVpdMap = std::get_if<types::IPZVpdMap>(&i_parsedVpdMap))
    {
        if (!i_fruJson["skipRecords"].is_array())
        {
            m_logger->logMessage(
                "Found invalid format for skipRecords in json, for inventory path :" +
                std::string(i_fruJson["inventoryPath"]));
            return;
        }

        const auto& l_skipList = i_fruJson["skipRecords"];

        for (const auto& [l_recordName, l_keywordMap] : *l_ipzVpdMap)
        {
            if (std::find(l_skipList.begin(), l_skipList.end(), l_recordName) !=
                l_skipList.end())
            {
                continue;
            }

            // If NOT skipped, populate the interface map
            populateIPZVPDpropertyMap(o_interfaces, l_keywordMap,
                                      constants::ipzVpdInf + l_recordName);
        }
    }
}

bool Worker::processRedundantPreAction(
    const nlohmann::json& i_configJson,
    const std::string& i_eepromFilePath) noexcept
{
    try
    {
        bool l_presenceState = false;
        const types::VPDMapVariant& parsedVpdMap = parseVpdFile(
            i_configJson, i_eepromFilePath, false, l_presenceState);
        return true;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format(
                "Process redundant path failed for EEPROM [{}], reason [{}] ",
                i_eepromFilePath, EventLogger::getErrorMsg(l_ex)),
            PlaceHolder::COLLECTION);
    }
    return false;
}

std::tuple<bool, std::string> Worker::collectFruVpd(
    const std::string& i_fruPath, const nlohmann::json& i_cfgJsonObj,
    uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    bool l_fruPresent = false;

    try
    {
        if (i_fruPath.empty())
        {
            o_errCode = error_code::INVALID_INPUT_PARAMETER;
            return std::make_tuple(l_fruPresent,
                                   constants::vpdCollectionFailed);
        }

        if (!i_cfgJsonObj.contains("frus"))
        {
            o_errCode = error_code::INVALID_JSON;
            return std::make_tuple(l_fruPresent,
                                   constants::vpdCollectionFailed);
        }

        if (!i_cfgJsonObj["frus"].contains(i_fruPath))
        {
            o_errCode = error_code::FRU_PATH_NOT_FOUND;
            return std::make_tuple(l_fruPresent,
                                   constants::vpdCollectionFailed);
        }

        const auto& l_parseResult = parseAndPublishVPD(i_cfgJsonObj, i_fruPath);
        l_fruPresent = std::get<1>(l_parseResult);

        if (std::get<0>(l_parseResult))
        {
            return std::make_tuple(l_fruPresent,
                                   constants::vpdCollectionCompleted);
        }
        else
        {
            return std::make_tuple(l_fruPresent,
                                   constants::vpdCollectionFailed);
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
        m_logger->logMessage(std::format(
            "Failed to process FRU VPD collection for path : {}, error : {}.",
            i_fruPath, l_ex.what()));

        return std::make_tuple(l_fruPresent, constants::vpdCollectionFailed);
    }
}

} // namespace vpd
