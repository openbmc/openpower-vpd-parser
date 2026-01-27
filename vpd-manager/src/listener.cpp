#include "listener.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "utility/common_utility.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/event_logger_utility.hpp"
#include "utility/json_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

namespace vpd
{
Listener::Listener(
    const std::shared_ptr<Worker>& i_worker,
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection) :
    m_worker(i_worker), m_asioConnection(i_asioConnection)
{
    if (m_worker == nullptr)
    {
        throw std::runtime_error(
            "Cannot instantiate Listener as Worker is not initialized");
    }
}

void Listener::registerHostStateChangeCallback() const noexcept
{
    try
    {
        static std::shared_ptr<sdbusplus::bus::match_t> l_hostState =
            std::make_shared<sdbusplus::bus::match_t>(
                *m_asioConnection,
                sdbusplus::bus::match::rules::propertiesChanged(
                    constants::hostObjectPath, constants::hostInterface),
                [this](sdbusplus::message_t& i_msg) {
                    hostStateChangeCallBack(i_msg);
                });
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0,
            "Register Host state change callback failed, reason: " +
                std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::hostStateChangeCallBack(
    sdbusplus::message_t& i_msg) const noexcept
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw std::runtime_error(
                "Error reading callback message for host state");
        }

        std::string l_objectPath;
        types::PropertyMap l_propMap;
        i_msg.read(l_objectPath, l_propMap);

        const auto l_itr = l_propMap.find("CurrentHostState");

        if (l_itr == l_propMap.end())
        {
            // CurrentHostState is not found in the callback message
            return;
        }

        if (auto l_hostState = std::get_if<std::string>(&(l_itr->second)))
        {
            // implies system is moving from standby to power on state
            if (*l_hostState == "xyz.openbmc_project.State.Host.HostState."
                                "TransitioningToRunning")
            {
                // TODO: check for all the essential FRUs in the system.

                if (m_worker.get() != nullptr)
                {
                    // Perform recollection.
                    m_worker->performVpdRecollection();
                }
                else
                {
                    logging::logMessage(
                        "Failed to get worker object, Abort re-collection");
                }
            }
        }
        else
        {
            throw std::runtime_error(
                "Invalid type recieved in variant for host state.");
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0,
            "Host state change callback failed, reason: " +
                std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::registerAssetTagChangeCallback() const noexcept
{
    try
    {
        static std::shared_ptr<sdbusplus::bus::match_t> l_assetMatch =
            std::make_shared<sdbusplus::bus::match_t>(
                *m_asioConnection,
                sdbusplus::bus::match::rules::propertiesChanged(
                    constants::systemInvPath, constants::assetTagInf),
                [this](sdbusplus::message_t& l_msg) {
                    assetTagChangeCallback(l_msg);
                });
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0,
            "Register AssetTag change callback failed, reason: " +
                std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::assetTagChangeCallback(
    sdbusplus::message_t& i_msg) const noexcept
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw std::runtime_error(
                "Error reading callback msg for asset tag.");
        }

        std::string l_objectPath;
        types::PropertyMap l_propMap;
        i_msg.read(l_objectPath, l_propMap);

        const auto& l_itrToAssetTag = l_propMap.find("AssetTag");
        if (l_itrToAssetTag != l_propMap.end())
        {
            if (auto l_assetTag =
                    std::get_if<std::string>(&(l_itrToAssetTag->second)))
            {
                // Call Notify to persist the AssetTag
                types::ObjectMap l_objectMap = {
                    {sdbusplus::message::object_path(constants::systemInvPath),
                     {{constants::assetTagInf, {{"AssetTag", *l_assetTag}}}}}};

                // Call method to update the dbus
                if (!dbusUtility::publishVpdOnDBus(move(l_objectMap)))
                {
                    throw std::runtime_error(
                        "Call to PIM failed for asset tag update.");
                }
            }
        }
        else
        {
            throw std::runtime_error(
                "Could not find asset tag in callback message.");
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0,
            "AssetTag update failed, reason: " + std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::registerPresenceChangeCallback() noexcept
{
    try
    {
        uint16_t l_errCode = 0;
        // get list of FRUs for which presence monitoring is required
        const auto& l_listOfFrus = jsonUtility::getFrusWithPresenceMonitoring(
            m_worker->getSysCfgJsonObj(), l_errCode);

        if (l_errCode)
        {
            logging::logMessage(
                "Failed to get list of FRUs with presence monitoring, error: " +
                commonUtility::getErrCodeMsg(l_errCode));
            return;
        }

        for (const auto& l_inventoryPath : l_listOfFrus)
        {
            std::shared_ptr<sdbusplus::bus::match_t> l_fruPresenceMatch =
                std::make_shared<sdbusplus::bus::match_t>(
                    *m_asioConnection,
                    sdbusplus::bus::match::rules::propertiesChanged(
                        l_inventoryPath, constants::inventoryItemInf),
                    [this](sdbusplus::message_t& i_msg) {
                        presentPropertyChangeCallback(i_msg);
                    });

            // save the match object to map
            m_fruPresenceMatchObjectMap[l_inventoryPath] = l_fruPresenceMatch;
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0,
            "Register presence change callback failed, reason: " +
                std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::presentPropertyChangeCallback(
    sdbusplus::message_t& i_msg) const noexcept
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw DbusException(
                "Error reading callback message for Present property change");
        }

        std::string l_interface;
        types::PropertyMap l_propMap;
        i_msg.read(l_interface, l_propMap);

        const std::string l_objectPath{i_msg.get_path()};

        const auto l_itr = l_propMap.find("Present");
        if (l_itr == l_propMap.end())
        {
            // Present is not found in the callback message
            return;
        }

        if (auto l_present = std::get_if<bool>(&(l_itr->second)))
        {
            *l_present ? m_worker->collectSingleFruVpd(l_objectPath)
                       : m_worker->deleteFruVpd(l_objectPath);
        }
        else
        {
            throw DbusException(
                "Invalid type recieved in variant for present property");
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0,
            "Process presence change callback failed, reason: " +
                std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::registerCorrPropCallBack(
    const std::string& i_correlatedPropJsonFile) noexcept
{
    try
    {
        uint16_t l_errCode = 0;
        m_correlatedPropJson =
            jsonUtility::getParsedJson(i_correlatedPropJsonFile, l_errCode);

        if (l_errCode)
        {
            throw JsonException("Failed to parse correlated properties JSON [" +
                                    i_correlatedPropJsonFile + "], error : " +
                                    commonUtility::getErrCodeMsg(l_errCode),
                                i_correlatedPropJsonFile);
        }

        const nlohmann::json& l_serviceJsonObjectList =
            m_correlatedPropJson.get_ref<const nlohmann::json::object_t&>();

        // Iterate through all services in the correlated properties json
        for (const auto& l_serviceJsonObject : l_serviceJsonObjectList.items())
        {
            const auto& l_serviceName = l_serviceJsonObject.key();

            const nlohmann::json& l_correlatedIntfJsonObj =
                m_correlatedPropJson[l_serviceName]
                    .get_ref<const nlohmann::json::object_t&>();

            // register properties changed D-Bus signal callback
            // for all interfaces under this service.
            std::for_each(l_correlatedIntfJsonObj.items().begin(),
                          l_correlatedIntfJsonObj.items().end(),
                          [this, &l_serviceName = std::as_const(l_serviceName)](
                              const auto& i_interfaceJsonObj) {
                              registerPropChangeCallBack(
                                  l_serviceName, i_interfaceJsonObj.key(),
                                  [this](sdbusplus::message_t& i_msg) {
                                      correlatedPropChangedCallBack(i_msg);
                                  });
                          });
        } // service loop
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::registerPropChangeCallBack(
    const std::string& i_service, const std::string& i_interface,
    std::function<void(sdbusplus::message_t& i_msg)> i_callBackFunction)
{
    try
    {
        if (i_service.empty() || i_interface.empty())
        {
            throw std::runtime_error("Invalid service name or interface name");
        }

        std::shared_ptr<sdbusplus::bus::match_t> l_matchObj =
            std::make_unique<sdbusplus::bus::match_t>(
                static_cast<sdbusplus::bus_t&>(*m_asioConnection),
                "type='signal',member='PropertiesChanged',"
                "interface='org.freedesktop.DBus.Properties',"
                "arg0='" +
                    i_interface + "'",
                i_callBackFunction);

        // save the match object in map
        m_matchObjectMap[i_service][i_interface] = l_matchObj;
    }
    catch (const std::exception& l_ex)
    {
        throw FirmwareException(l_ex.what());
    }
}

void Listener::correlatedPropChangedCallBack(
    sdbusplus::message_t& i_msg) noexcept
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw DbusException("Error in reading property change signal.");
        }

        std::string l_interface;
        types::PropertyMap l_propMap;
        i_msg.read(l_interface, l_propMap);

        const std::string l_objectPath{i_msg.get_path()};

        std::string l_serviceName =
            dbusUtility::getServiceNameFromConnectionId(i_msg.get_sender());

        if (l_serviceName.empty())
        {
            throw DbusException(
                "Failed to get service name from connection ID: " +
                std::string(i_msg.get_sender()));
        }

        // if service name contains .service suffix, strip it
        const std::size_t l_pos = l_serviceName.find(".service");
        if (l_pos != std::string::npos)
        {
            l_serviceName = l_serviceName.substr(0, l_pos);
        }

        // iterate through all properties in map
        for (const auto& l_propertyEntry : l_propMap)
        {
            const std::string& l_propertyName = l_propertyEntry.first;
            const auto& l_propertyValue = l_propertyEntry.second;

            // Use correlated JSON to find target {object path,
            // interface,property/properties} to update
            const auto& l_correlatedPropList = getCorrelatedProps(
                l_serviceName, l_objectPath, l_interface, l_propertyName);

            // update all target correlated properties
            std::for_each(
                l_correlatedPropList.begin(), l_correlatedPropList.end(),
                [this, &l_propertyValue = std::as_const(l_propertyValue),
                 &l_serviceName = std::as_const(l_serviceName),
                 &l_objectPath = std::as_const(l_objectPath),
                 &l_interface = std::as_const(l_interface),
                 &l_propertyName = std::as_const(l_propertyName)](
                    const auto& i_corrProperty) {
                    if (!updateCorrelatedProperty(l_serviceName, i_corrProperty,
                                                  l_propertyValue))
                    {
                        logging::logMessage(
                            "Failed to update correlated property: " +
                            l_serviceName + " : " +
                            std::get<0>(i_corrProperty) + " : " +
                            std::get<1>(i_corrProperty) + " : " +
                            std::get<2>(i_corrProperty) + " when " +
                            l_objectPath + " : " + l_interface + " : " +
                            l_propertyName + " got updated.");
                    }
                });
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

types::DbusPropertyList Listener::getCorrelatedProps(
    const std::string& i_serviceName, const std::string& i_objectPath,
    const std::string& i_interface, const std::string& i_property) const
{
    types::DbusPropertyList l_result;
    try
    {
        if (m_correlatedPropJson.contains(i_serviceName) &&
            m_correlatedPropJson[i_serviceName].contains(i_interface) &&
            m_correlatedPropJson[i_serviceName][i_interface].contains(
                i_property))
        {
            const nlohmann::json& l_destinationJsonObj =
                m_correlatedPropJson[i_serviceName][i_interface][i_property];

            // check if any matching paths pair entry is present
            if (l_destinationJsonObj.contains("pathsPair") &&
                l_destinationJsonObj["pathsPair"].contains(i_objectPath) &&
                l_destinationJsonObj["pathsPair"][i_objectPath].contains(
                    "destinationInventoryPath") &&
                l_destinationJsonObj["pathsPair"][i_objectPath].contains(
                    "interfaces"))
            {
                // iterate through all the destination interface and property
                // name
                for (const auto& l_destinationInterfaceJsonObj :
                     l_destinationJsonObj["pathsPair"][i_objectPath]
                                         ["interfaces"]
                                             .items())
                {
                    // iterate through all destination inventory paths
                    for (const auto& l_destinationInventoryPath :
                         l_destinationJsonObj["pathsPair"][i_objectPath]
                                             ["destinationInventoryPath"])
                    {
                        l_result.emplace_back(
                            l_destinationInventoryPath,
                            l_destinationInterfaceJsonObj.key(),
                            l_destinationInterfaceJsonObj.value());
                    } // destination inventory paths
                } // destination interfaces
            }
            // get the default interface, property to update
            else if (l_destinationJsonObj.contains("defaultInterfaces"))
            {
                // iterate through all default interfaces to update
                for (const auto& l_destinationIfcPropEntry :
                     l_destinationJsonObj["defaultInterfaces"].items())
                {
                    l_result.emplace_back(i_objectPath,
                                          l_destinationIfcPropEntry.key(),
                                          l_destinationIfcPropEntry.value());
                }
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        throw FirmwareException(l_ex.what());
    }
    return l_result;
}

bool Listener::updateCorrelatedProperty(
    const std::string& i_serviceName,
    const types::DbusPropertyEntry& i_corrProperty,
    const types::DbusVariantType& i_propertyValue) const noexcept
{
    const auto& l_destinationObjectPath{std::get<0>(i_corrProperty)};
    const auto& l_destinationInterface{std::get<1>(i_corrProperty)};
    const auto& l_destinationPropertyName{std::get<2>(i_corrProperty)};

    try
    {
        types::DbusVariantType l_valueToUpdate;

        // destination interface is ipz vpd
        if (l_destinationInterface.find(constants::ipzVpdInf) !=
            std::string::npos)
        {
            uint16_t l_errCode = 0;
            if (const auto l_val = std::get_if<std::string>(&i_propertyValue))
            {
                // convert value to binary vector before updating
                l_valueToUpdate =
                    commonUtility::convertToBinary(*l_val, l_errCode);

                if (l_errCode)
                {
                    throw std::runtime_error(
                        "Failed to get value [" + std::string(*l_val) +
                        "] in binary vector, error : " +
                        commonUtility::getErrCodeMsg(l_errCode));
                }
            }
            else if (const auto l_val =
                         std::get_if<types::BinaryVector>(&i_propertyValue))
            {
                l_valueToUpdate = *l_val;
            }
        }
        else
        {
            // destination interface is not ipz vpd, assume target
            // property type is of string type
            if (const auto l_val =
                    std::get_if<types::BinaryVector>(&i_propertyValue))
            {
                // convert property value to string before updating
                uint16_t l_errCode = 0;
                l_valueToUpdate =
                    commonUtility::getPrintableValue(*l_val, l_errCode);

                if (l_errCode)
                {
                    throw std::runtime_error(
                        "Failed to get binary value in string, error : " +
                        commonUtility::getErrCodeMsg(l_errCode));
                }
            }
            else if (const auto l_val =
                         std::get_if<std::string>(&i_propertyValue))
            {
                l_valueToUpdate = *l_val;
            }
        }

        if (i_serviceName == constants::pimServiceName)
        {
            // Call dbus method to update on dbus
            return dbusUtility::publishVpdOnDBus(types::ObjectMap{
                {l_destinationObjectPath,
                 {{l_destinationInterface,
                   {{l_destinationPropertyName, l_valueToUpdate}}}}}});
        }
        else
        {
            return dbusUtility::writeDbusProperty(
                i_serviceName, l_destinationObjectPath, l_destinationInterface,
                l_destinationPropertyName, l_valueToUpdate);
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to update correlated property: " + i_serviceName + " : " +
            l_destinationObjectPath + " : " + l_destinationInterface + " : " +
            l_destinationPropertyName + ". Error: " + std::string(l_ex.what()));
    }
    return false;
}

} // namespace vpd
