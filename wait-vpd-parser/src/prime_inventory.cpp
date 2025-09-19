#include "prime_inventory.hpp"

#include "logger.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <string>

#define SYSTEM_VPD_FILE_PATH "/sys/bus/i2c/drivers/at24/8-0050/eeprom"

PrimeInventory::PrimeInventory(const std::string& i_sysConfJsonPath)
{
    uint16_t l_errCode = 0;
    m_sysCfgJsonObj =
        vpd::jsonUtility::getParsedJson(i_sysConfJsonPath, l_errCode);

    if (l_errCode)
    {
        throw std::runtime_error(
            "JSON parsing failed for file [ " + i_sysConfJsonPath +
            " ], error : " + vpd::vpdSpecificUtility::getErrCodeMsg(l_errCode));
    }

    // check for mandatory fields at this point itself.
    if (!m_sysCfgJsonObj.contains("frus"))
    {
        throw std::runtime_error("Mandatory tag(s) missing from JSON");
    }

    m_logger = vpd::Logger::getLoggerInstance();
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

    std::cout << "Prime done for : " << i_vpdFilePath << std::endl;
    return true;
}

void PrimeInventory::populateInterfaces(
    const nlohmann::json& interfaceJson, vpd::types::InterfaceMap& interfaceMap,
    const vpd::types::VPDMapVariant& parsedVpdMap) const noexcept
{
    for (const auto& interfacesPropPair : interfaceJson.items())
    {
        const std::string& interface = interfacesPropPair.key();
        vpd::types::PropertyMap propertyMap;

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
                        vpd::vpdSpecificUtility::getExpandedLocationCode(
                            propValuePair.value().get<std::string>(),
                            parsedVpdMap);
                    propertyMap.emplace(property, value);

                    auto l_locCodeProperty = propertyMap;
                    vpd::vpdSpecificUtility::insertOrMerge(
                        interfaceMap,
                        std::string(vpd::constants::xyzLocationCodeInf),
                        move(l_locCodeProperty));
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
                        propValuePair.value().get<vpd::types::BinaryVector>());
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
                        std::get_if<vpd::types::IPZVpdMap>(&parsedVpdMap))
                {
                    if (!record.empty() && !keyword.empty() &&
                        (*ipzVpdMap).count(record) &&
                        (*ipzVpdMap).at(record).count(keyword))
                    {
                        auto encoded = vpd::vpdSpecificUtility::encodeKeyword(
                            ((*ipzVpdMap).at(record).at(keyword)), encoding);
                        propertyMap.emplace(property, encoded);
                    }
                }
                else if (auto kwdVpdMap =
                             std::get_if<vpd::types::KeywordVpdMap>(
                                 &parsedVpdMap))
                {
                    if (!keyword.empty() && (*kwdVpdMap).count(keyword))
                    {
                        if (auto kwValue =
                                std::get_if<vpd::types::BinaryVector>(
                                    &(*kwdVpdMap).at(keyword)))
                        {
                            auto encodedValue =
                                vpd::vpdSpecificUtility::encodeKeyword(
                                    std::string((*kwValue).begin(),
                                                (*kwValue).end()),
                                    encoding);

                            propertyMap.emplace(property, encodedValue);
                        }
                        else if (auto kwValue = std::get_if<std::string>(
                                     &(*kwdVpdMap).at(keyword)))
                        {
                            auto encodedValue =
                                vpd::vpdSpecificUtility::encodeKeyword(
                                    std::string((*kwValue).begin(),
                                                (*kwValue).end()),
                                    encoding);

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
        vpd::vpdSpecificUtility::insertOrMerge(interfaceMap, interface,
                                               move(propertyMap));
    }
}

void PrimeInventory::processFunctionalProperty(
    const std::string& i_inventoryObjPath,
    vpd::types::InterfaceMap& io_interfaces) const noexcept
{
    if (!vpd::dbusUtility::isChassisPowerOn())
    {
        std::array<const char*, 1> l_operationalStatusInf = {
            vpd::constants::operationalStatusInf};

        auto mapperObjectMap = vpd::dbusUtility::getObjectMap(
            i_inventoryObjPath, l_operationalStatusInf);

        // If the object has been found. Check if it is under PIM.
        if (mapperObjectMap.size() != 0)
        {
            for (const auto& [l_serviceName, l_interfaceLsit] : mapperObjectMap)
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
        std::array<const char*, 1> l_enableInf = {vpd::constants::enableInf};

        auto mapperObjectMap =
            vpd::dbusUtility::getObjectMap(i_inventoryObjPath, l_enableInf);

        // If the object has been found. Check if it is under PIM.
        if (mapperObjectMap.size() != 0)
        {
            for (const auto& [l_serviceName, l_interfaceLsit] : mapperObjectMap)
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
