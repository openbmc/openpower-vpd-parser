#include "listener.hpp"

#include "event_logger.hpp"
#include "utility/dbus_utility.hpp"

namespace vpd
{
Listener::Listener(
    const std::shared_ptr<Worker>& i_worker,
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection) :
    m_worker(i_worker), m_asioConnection(i_asioConnection)
{
    registerHostStateChangeCallback();
    registerAssetTagChangeCallback();
}

void Listener::registerHostStateChangeCallback()
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

void Listener::hostStateChangeCallBack(sdbusplus::message_t& i_msg)
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
            throw std::runtime_error(
                "CurrentHostState field is missing in callback message");
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
        // TODO: Log PEL.
        logging::logMessage(l_ex.what());
    }
}

void Listener::registerAssetTagChangeCallback()
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

void Listener::assetTagChangeCallback(sdbusplus::message_t& i_msg)
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
        // TODO: Log PEL with below description.
        logging::logMessage("Asset tag callback update failed with error: " +
                            std::string(l_ex.what()));
    }
}

} // namespace vpd
