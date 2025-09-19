#include "prime_inventory.hpp"

#include "event_logger.hpp"
#include "exceptions.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/json_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <string>
#include <vector>

PrimeInventory::PrimeInventory()
{
    try
    {
        uint16_t l_errCode = 0;
        m_sysCfgJsonObj =
            vpd::jsonUtility::getParsedJson(INVENTORY_JSON_SYM_LINK, l_errCode);

        if (l_errCode)
        {
            throw std::runtime_error(
                "JSON parsing failed for file [ " +
                std::string(INVENTORY_JSON_SYM_LINK) + " ], error : " +
                vpd::vpdSpecificUtility::getErrCodeMsg(l_errCode));
        }

        // check for mandatory fields at this point itself.
        if (!m_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error(
                "Mandatory tag(s) missing from JSON file [" +
                std::string(INVENTORY_JSON_SYM_LINK) + "]");
        }

        m_logger = vpd::Logger::getLoggerInstance();
    }
    catch (const std::exception& l_ex)
    {
        vpd::EventLogger::createSyncPel(
            vpd::types::ErrorType::JsonFailure,
            vpd::types::SeverityType::Critical, __FILE__, __FUNCTION__, 0,
            "Prime inventory failed, reason: " + std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);

        throw;
    }
}

bool PrimeInventory::isPrimingRequired() const noexcept
{
    try
    {
        // get all object paths under PIM
        const auto l_objectPaths = vpd::dbusUtility::GetSubTreePaths(
            vpd::constants::systemInvPath, 0,
            std::vector<std::string>{vpd::constants::vpdCollectionInterface});

        const nlohmann::json& l_listOfFrus =
            m_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

        size_t l_invPathCount = 0;

        for (const auto& l_itemFRUS : l_listOfFrus.items())
        {
            for (const auto& l_Fru : l_itemFRUS.value())
            {
                if (l_Fru.contains("ccin") || (l_Fru.contains("noprime") &&
                                               l_Fru.value("noprime", false)))
                {
                    continue;
                }

                l_invPathCount += 1;
            }
        }
        return ((l_objectPaths.size() >= l_invPathCount) ? false : true);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Error while checking is priming required or not, error: " +
            std::string(l_ex.what()));
    }

    // In case of any error, perform priming, as it's unclear whether priming is
    // required.
    return true;
}

void PrimeInventory::primeSystemBlueprint() const noexcept
{
    try
    {
        if (m_sysCfgJsonObj.empty())
        {
            return;
        }

        const nlohmann::json& l_listOfFrus =
            m_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

        for (const auto& l_itemFRUS : l_listOfFrus.items())
        {
            const std::string& l_vpdFilePath = l_itemFRUS.key();

            if (l_vpdFilePath == SYSTEM_VPD_FILE_PATH)
            {
                continue;
            }

            // Prime the inventry for FRUs
            if (!primeInventory(l_vpdFilePath))
            {
                m_logger->logMessage(
                    "Priming of inventory failed for FRU " + l_vpdFilePath);
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        // ToDo: log an error
    }
}

bool PrimeInventory::primeInventory(
    const std::string& i_vpdFilePath) const noexcept
{
    if (i_vpdFilePath.empty())
    {
        m_logger->logMessage("Empty VPD file path given");
        return false;
    }

    if (m_sysCfgJsonObj.empty())
    {
        m_logger->logMessage("Empty JSON detected for " + i_vpdFilePath);
        return false;
    }
    else if (!m_sysCfgJsonObj["frus"].contains(i_vpdFilePath))
    {
        m_logger->logMessage("File " + i_vpdFilePath +
                             ", is not found in the system config JSON file.");
        return false;
    }

    vpd::types::ObjectMap l_objectInterfaceMap;
    for (const auto& l_Fru : m_sysCfgJsonObj["frus"][i_vpdFilePath])
    {
        vpd::types::InterfaceMap l_interfaces;
        sdbusplus::message::object_path l_fruObjectPath(l_Fru["inventoryPath"]);

        if (l_Fru.contains("ccin"))
        {
            continue;
        }

        if (l_Fru.contains("noprime") && l_Fru.value("noprime", false))
        {
            continue;
        }

        // Reset data under PIM for this FRU only if the FRU is not synthesized
        // and we handle it's Present property.
        if (isPresentPropertyHandlingRequired(l_Fru))
        {
            // Clear data under PIM if already exists.
            vpd::vpdSpecificUtility::resetDataUnderPIM(
                std::string(l_Fru["inventoryPath"]), l_interfaces);
        }

        // Add extra interfaces mentioned in the Json config file
        if (l_Fru.contains("extraInterfaces"))
        {
            populateInterfaces(l_Fru["extraInterfaces"], l_interfaces,
                               std::monostate{});
        }

        vpd::types::PropertyMap l_propertyValueMap;

        // Update Present property for this FRU only if we handle Present
        // property for the FRU.
        if (isPresentPropertyHandlingRequired(l_Fru))
        {
            l_propertyValueMap.emplace("Present", false);

            // TODO: Present based on file will be taken care in future.
            // By default present is set to false for FRU at the time of
            // priming. Once collection goes through, it will be set to true in
            // that flow.
            /*if (std::filesystem::exists(i_vpdFilePath))
            {
                l_propertyValueMap["Present"] = true;
            }*/
        }

        vpd::vpdSpecificUtility::insertOrMerge(
            l_interfaces, "xyz.openbmc_project.Inventory.Item",
            move(l_propertyValueMap));

        if (l_Fru.value("inherit", true) &&
            m_sysCfgJsonObj.contains("commonInterfaces"))
        {
            populateInterfaces(m_sysCfgJsonObj["commonInterfaces"],
                               l_interfaces, std::monostate{});
        }

        processFunctionalProperty(l_Fru["inventoryPath"], l_interfaces);
        processEnabledProperty(l_Fru["inventoryPath"], l_interfaces);

        // Emplace the default state of FRU VPD collection
        vpd::types::PropertyMap l_fruCollectionProperty = {
            {"Status", vpd::constants::vpdCollectionNotStarted}};

        vpd::vpdSpecificUtility::insertOrMerge(
            l_interfaces, vpd::constants::vpdCollectionInterface,
            std::move(l_fruCollectionProperty));

        l_objectInterfaceMap.emplace(std::move(l_fruObjectPath),
                                     std::move(l_interfaces));
    }

    // Notify PIM
    if (!vpd::dbusUtility::callPIM(move(l_objectInterfaceMap)))
    {
        m_logger->logMessage(
            "Call to PIM failed for VPD file " + i_vpdFilePath);
        return false;
    }

    return true;
}

void PrimeInventory::populateInterfaces(
    const nlohmann::json& i_interfaceJson,
    vpd::types::InterfaceMap& io_interfaceMap,
    const vpd::types::VPDMapVariant& i_parsedVpdMap) const noexcept
{
    for (const auto& l_interfacesPropPair : i_interfaceJson.items())
    {
        const std::string& l_interface = l_interfacesPropPair.key();
        vpd::types::PropertyMap l_propertyMap;

        for (const auto& l_propValuePair : l_interfacesPropPair.value().items())
        {
            const std::string l_property = l_propValuePair.key();

            if (l_propValuePair.value().is_boolean())
            {
                l_propertyMap.emplace(l_property,
                                      l_propValuePair.value().get<bool>());
            }
            else if (l_propValuePair.value().is_string())
            {
                if (l_property.compare("LocationCode") == 0 &&
                    l_interface.compare("com.ibm.ipzvpd.Location") == 0)
                {
                    std::string l_value =
                        vpd::vpdSpecificUtility::getExpandedLocationCode(
                            l_propValuePair.value().get<std::string>(),
                            i_parsedVpdMap);
                    l_propertyMap.emplace(l_property, l_value);

                    auto l_locCodeProperty = l_propertyMap;
                    vpd::vpdSpecificUtility::insertOrMerge(
                        io_interfaceMap,
                        std::string(vpd::constants::xyzLocationCodeInf),
                        move(l_locCodeProperty));
                }
                else
                {
                    l_propertyMap.emplace(
                        l_property, l_propValuePair.value().get<std::string>());
                }
            }
            else if (l_propValuePair.value().is_array())
            {
                try
                {
                    l_propertyMap.emplace(l_property,
                                          l_propValuePair.value()
                                              .get<vpd::types::BinaryVector>());
                }
                catch (const nlohmann::detail::type_error& e)
                {
                    std::cerr << "Type exception: " << e.what() << "\n";
                }
            }
            else if (l_propValuePair.value().is_number())
            {
                // For now assume the value is a size_t.  In the future it would
                // be nice to come up with a way to get the type from the JSON.
                l_propertyMap.emplace(l_property,
                                      l_propValuePair.value().get<size_t>());
            }
            else if (l_propValuePair.value().is_object())
            {
                const std::string& l_record =
                    l_propValuePair.value().value("recordName", "");
                const std::string& l_keyword =
                    l_propValuePair.value().value("keywordName", "");
                const std::string& l_encoding =
                    l_propValuePair.value().value("encoding", "");

                if (auto l_ipzVpdMap =
                        std::get_if<vpd::types::IPZVpdMap>(&i_parsedVpdMap))
                {
                    if (!l_record.empty() && !l_keyword.empty() &&
                        (*l_ipzVpdMap).count(l_record) &&
                        (*l_ipzVpdMap).at(l_record).count(l_keyword))
                    {
                        auto l_encoded = vpd::vpdSpecificUtility::encodeKeyword(
                            ((*l_ipzVpdMap).at(l_record).at(l_keyword)),
                            l_encoding);
                        l_propertyMap.emplace(l_property, l_encoded);
                    }
                }
                else if (auto l_kwdVpdMap =
                             std::get_if<vpd::types::KeywordVpdMap>(
                                 &i_parsedVpdMap))
                {
                    if (!l_keyword.empty() && (*l_kwdVpdMap).count(l_keyword))
                    {
                        if (auto l_kwValue =
                                std::get_if<vpd::types::BinaryVector>(
                                    &(*l_kwdVpdMap).at(l_keyword)))
                        {
                            auto l_encodedValue =
                                vpd::vpdSpecificUtility::encodeKeyword(
                                    std::string((*l_kwValue).begin(),
                                                (*l_kwValue).end()),
                                    l_encoding);

                            l_propertyMap.emplace(l_property, l_encodedValue);
                        }
                        else if (auto l_kwValue = std::get_if<std::string>(
                                     &(*l_kwdVpdMap).at(l_keyword)))
                        {
                            auto l_encodedValue =
                                vpd::vpdSpecificUtility::encodeKeyword(
                                    std::string((*l_kwValue).begin(),
                                                (*l_kwValue).end()),
                                    l_encoding);

                            l_propertyMap.emplace(l_property, l_encodedValue);
                        }
                        else if (auto l_uintValue = std::get_if<size_t>(
                                     &(*l_kwdVpdMap).at(l_keyword)))
                        {
                            l_propertyMap.emplace(l_property, *l_uintValue);
                        }
                        else
                        {
                            m_logger->logMessage(
                                "Unknown keyword found, Keywrod = " +
                                l_keyword);
                        }
                    }
                }
            }
        }
        vpd::vpdSpecificUtility::insertOrMerge(io_interfaceMap, l_interface,
                                               move(l_propertyMap));
    }
}

void PrimeInventory::processFunctionalProperty(
    const std::string& i_inventoryObjPath,
    vpd::types::InterfaceMap& io_interfaces) const noexcept
{
    if (!vpd::dbusUtility::isChassisPowerOn())
    {
        std::vector<std::string> l_operationalStatusInf{
            vpd::constants::operationalStatusInf};

        auto l_mapperObjectMap = vpd::dbusUtility::getObjectMap(
            i_inventoryObjPath, l_operationalStatusInf);

        // If the object has been found. Check if it is under PIM.
        if (l_mapperObjectMap.size() != 0)
        {
            for (const auto& [l_serviceName, l_interfaceLsit] :
                 l_mapperObjectMap)
            {
                if (l_serviceName == vpd::constants::pimServiceName)
                {
                    // The object is already under PIM. No need to process
                    // again. Retain the old value.
                    return;
                }
            }
        }

        // Implies value is not there in D-Bus. Populate it with default
        // value "true".
        vpd::types::PropertyMap l_functionalProp;
        l_functionalProp.emplace("Functional", true);
        vpd::vpdSpecificUtility::insertOrMerge(
            io_interfaces, vpd::constants::operationalStatusInf,
            move(l_functionalProp));
    }

    // if chassis is power on. Functional property should be there on D-Bus.
    // Don't process.
    return;
}

void PrimeInventory::processEnabledProperty(
    const std::string& i_inventoryObjPath,
    vpd::types::InterfaceMap& io_interfaces) const noexcept
{
    if (!vpd::dbusUtility::isChassisPowerOn())
    {
        std::vector<std::string> l_enableInf{vpd::constants::enableInf};

        auto l_mapperObjectMap =
            vpd::dbusUtility::getObjectMap(i_inventoryObjPath, l_enableInf);

        // If the object has been found. Check if it is under PIM.
        if (l_mapperObjectMap.size() != 0)
        {
            for (const auto& [l_serviceName, l_interfaceLsit] :
                 l_mapperObjectMap)
            {
                if (l_serviceName == vpd::constants::pimServiceName)
                {
                    // The object is already under PIM. No need to process
                    // again. Retain the old value.
                    return;
                }
            }
        }

        // Implies value is not there in D-Bus. Populate it with default
        // value "true".
        vpd::types::PropertyMap l_enabledProp;
        l_enabledProp.emplace("Enabled", true);
        vpd::vpdSpecificUtility::insertOrMerge(
            io_interfaces, vpd::constants::enableInf, move(l_enabledProp));
    }

    // if chassis is power on. Enabled property should be there on D-Bus.
    // Don't process.
    return;
}
