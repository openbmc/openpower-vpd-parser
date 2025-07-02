#include "listener.hpp"

#include "constants.hpp"
#include "event_logger.hpp"
#include "utility/dbus_utility.hpp"

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
        const auto& l_parsedSysCfgJson = m_worker->getSysCfgJsonObj();
        if (!l_parsedSysCfgJson.contains("frus"))
        {
            throw JsonException("Invalid system config JSON.");
        }

        const nlohmann::json& l_listOfFrus =
            l_parsedSysCfgJson["frus"]
                .get_ref<const nlohmann::json::object_t&>();

        for (const auto& l_aFru : l_listOfFrus)
        {
            if (l_aFru.at(0).value("monitorPresence", false))
            {
                const std::string& l_inventoryPath =
                    l_aFru.at(0).value("inventoryPath", "");

                std::shared_ptr<sdbusplus::bus::match_t> l_fruPresenceMatch =
                    std::make_shared<sdbusplus::bus::match_t>(
                        *m_asioConnection,
                        sdbusplus::bus::match::rules::propertiesChanged(
                            l_inventoryPath, constants::inventoryItemInf),
                        [this](sdbusplus::message_t& i_msg) {
                            presentPropertyChangeCallBack(i_msg);
                        });

                // save the match object to map
                m_fruPresenceMatchObjectMap[l_inventoryPath] =
                    l_fruPresenceMatch;
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Warning,
            __FILE__, __FUNCTION__, 0,
            "Register presence change callback failed, reason: " +
                std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::presentPropertyChangeCallBack(
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

        (void)l_interface;

        std::string l_objectPath{i_msg.get_path()};

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
            EventLogger::getErrorType(l_ex), types::SeverityType::Warning,
            __FILE__, __FUNCTION__, 0,
            "Process presence change callback failed, reason: " +
                std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

} // namespace vpd
