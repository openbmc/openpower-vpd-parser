#include "listener.hpp"

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
        /* TODO:
            - iterate through all FRUs.
            - if FRU is runtime replaceable and we do not handle presence for
           the FRU, register a Present property change callback.
        */
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

        const auto& l_objectPath = i_msg.get_path();
        (void)l_objectPath;
        /*TODO:
         - read "Present" property
         - if "Present" property = true, trigger "collectSingleFruVpd" for the
         FRU
         - if "Present" property = false, trigger "deleteFruVpd" for the FRU
        */
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
