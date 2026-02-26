#include "config.h"

#include "collection_orchestrator.hpp"

#include "exceptions.hpp"
#include "utility/dbus_utility.hpp"

#include <format>

void CollectionOrchestrator::triggerFruVpdCollectionAndCheckStatus()
{
    try
    {
        m_asioConn->async_method_call(
            [this](boost::system::error_code l_ec,
                   sdbusplus::message::message& l_msg) {
                this->collectAllFruVpdCallback(l_ec, l_msg);
            },
            m_collectionServiceName, m_objectPath, m_interface,
            m_collectionMethodName);

        // start the event loop with a timeout
        m_ioContext.run_for(m_timeout);

        // event loop returned
        if (m_collectionDone)
        {
            m_logger->logMessage("VPD collection is done");
        }
        else
        {
            throw std::runtime_error(
                "Timed out waiting for VPD collection status");
        }
    }
    catch (const std::exception& l_ex)
    {
        m_ioContext.stop();

        throw std::runtime_error(std::format(
            "CollectionOrchestrator failed. Error: {}", l_ex.what()));
    }
}

void CollectionOrchestrator::collectAllFruVpdCallback(
    const boost::system::error_code i_ec, sdbusplus::message_t& i_msg)
{
    try
    {
        if (i_ec == boost::system::errc::timed_out)
        {
            throw vpd::DbusException("CollectAllFruVpd timed out");
        }
        else if (i_ec || i_msg.is_method_error())
        {
            throw vpd::DbusException(std::format(
                "CollectAllFruVpd failed. Error : {}", i_ec.message()));
        }

        bool l_collectionTriggered{false};

        // D-Bus call has returned, read the return value
        i_msg.read(l_collectionTriggered);

        if (!l_collectionTriggered)
        {
            throw std::runtime_error("CollectAllFruVpd returned false");
        }

        m_logger->logMessage("CollectAllFRUVPD successful");

        // triggering FRU VPD collection is successful, now
        // start a Listener on the Collection Status
        // property
        registerVpdCollectionStatusListener();

        // read VPD Collection Status property from D-Bus and exit early if
        // already done
        readCollectionStatusProperty();

        if (m_collectionDone)
        {
            // destruct the match object to stop the listener on Collection
            // Status property
            m_collectionStatusMatch.reset();

            m_logger->logMessage(
                "Collection status property has been read as completed ");
            m_ioContext.stop();
        }
    }
    catch (const std::exception& l_ex)
    {
        m_ioContext.stop();
        throw std::runtime_error(std::format(
            "Error processing collect all FRU VPD callback: {}", l_ex.what()));
    }
}

void CollectionOrchestrator::registerVpdCollectionStatusListener()
{
    try
    {
        m_collectionStatusMatch = std::make_unique<sdbusplus::bus::match_t>(
            *m_asioConn,
            sdbusplus::bus::match::rules::propertiesChanged(
                OBJPATH, vpd::constants::vpdCollectionInterface),
            [this](sdbusplus::message_t& l_msg) {
                vpdCollectionStatusCallback(l_msg);
            });
    }
    catch (const std::exception& l_ex)
    {
        throw std::runtime_error(std::format(
            "Failed to register listener on FRU VPD collection status property. Error: {}",
            l_ex.what()));
    }
}

void CollectionOrchestrator::vpdCollectionStatusCallback(
    sdbusplus::message_t& i_msg)
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw vpd::DbusException(
                "Error in reading VPD collection status signal. ");
        }

        std::string l_objectPath;
        vpd::types::PropertyMap l_propMap;
        i_msg.read(l_objectPath, l_propMap);

        const auto l_itr = l_propMap.find("Status");

        if (l_itr == l_propMap.end())
        {
            // "Status" is not found in the callback message
            return;
        }

        if (auto l_collectionStatus = std::get_if<std::string>(&l_itr->second))
        {
            if (*l_collectionStatus == vpd::constants::vpdCollectionCompleted)
            {
                // stop the event loop
                m_ioContext.stop();

                // set the collection flag
                m_collectionDone = true;
            }
            else
            {
                m_logger->logMessage("VPD collection not yet complete");
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to process VPD collection status callback. Error : {}",
            l_ex.what()));
    }
}

void CollectionOrchestrator::readCollectionStatusProperty()
{
    try
    {
        auto l_retVal = vpd::dbusUtility::readDbusProperty(
            BUSNAME, OBJPATH, vpd::constants::vpdCollectionInterface, "Status");
        if (auto l_collectionStatusProp = std::get_if<std::string>(&l_retVal))
        {
            m_collectionDone = (*l_collectionStatusProp ==
                                vpd::constants::vpdCollectionCompleted);
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to read VPD collection status. Error : {}", l_ex.what()));
    }
}
