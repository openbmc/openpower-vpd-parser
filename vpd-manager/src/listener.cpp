#include "listener.hpp"

#include "constants.hpp"
#include "event_logger.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/json_utility.hpp"

namespace vpd
{
Listener::Listener(
    const std::shared_ptr<Worker>& i_worker,
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection) :
    m_worker(i_worker), m_asioConnection(i_asioConnection)
{}

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
        logging::logMessage(
            "Register Host state change callback failed, reason: " +
            std::string(l_ex.what()));
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
        logging::logMessage(
            "Register AssetTag change callback failed, reason: " +
            std::string(l_ex.what()));
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

                // Notify PIM
                if (!dbusUtility::callPIM(move(l_objectMap)))
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
        // get list of FRUs for which presence monitoring is required
        const auto& l_listOfFrus = jsonUtility::getFrusWithPresenceMonitoring(
            m_worker->getSysCfgJsonObj());

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
        m_correlatedPropJson =
            jsonUtility::getParsedJson(i_correlatedPropJsonFile);
        if (m_correlatedPropJson.empty())
        {
            throw JsonException("Failed to parse correlated properties JSON",
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

        const std::string l_interface{i_msg.get_interface()};
        const std::string l_objectPath{i_msg.get_path()};
        const std::string l_signature{i_msg.get_signature()};

        (void)l_interface;
        (void)l_objectPath;
        (void)l_signature;

        /*TODO:
        Use correlated JSON to find target {object path, interface,
        property/properties} to update*/
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

} // namespace vpd
