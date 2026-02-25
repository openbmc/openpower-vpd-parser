#include "config.h"

#include "fru_vpd_collection_manager.hpp"

#include "utility/dbus_utility.hpp"

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

#include <format>

int FruVpdCollectionManager::triggerFruVpdCollectionAndCheckStatus()
{
    int l_rc{vpd::constants::VALUE_1};
    try
    {
        constexpr uint64_t l_collectionTriggerTimeoutUs{
            1 * 60 * 1000000}; // 1 min
                               // timeout

        m_asioConn->async_method_call_timed(
            [this](boost::system::error_code l_ec,
                   sdbusplus::message_t& l_ret) {
                m_logger->logMessage("_SR async_send callback");

                if (l_ec == boost::system::errc::timed_out)
                {
                    throw vpd::DbusException(
                        "wait-vpd-parsers timed out trying to call \"CollectAllFRUVPD\"");
                }
                else if (l_ec || l_ret.is_method_error())
                {
                    throw vpd::DbusException(
                        "Error with \"CollectAllFRUVPD\" async_send" +
                        l_ec.message());
                }
                else
                {
                    bool l_collectionTriggered{false};

                    // D-Bus call has returned
                    l_ret.read(l_collectionTriggered);

                    if (l_collectionTriggered)
                    {
                        m_logger->logMessage("CollectAllFRUVPD successful");

                        // start a timer to check if collection status is
                        // completed within a fixed time
                        m_collectionTimer =
                            make_shared<boost::asio::steady_timer>(
                                m_ioContext, m_collectionStatusTimeout);

                        m_collectionTimer->async_wait(boost::bind(
                            &FruVpdCollectionManager::handleTimerExpiry, this,
                            boost::asio::placeholders::error));

                        // triggering FRU VPD collection is successful, now
                        // start a Listener on the Collection Status
                        // property
                        registerVpdCollectionStatusListener();
                    }
                    else
                    {
                        m_logger->logMessage("CollectAllFRUVPD failed");

                        // need to fail the service
                        throw vpd::DbusException(
                            "Failed to trigger FRU VPD collection");
                    }
                }
            },
            m_collectionServiceName, m_objectPath, m_interface,
            m_collectionMethodName, l_collectionTriggerTimeoutUs);

        // start the event loop
        m_ioContext.run();

        // I/O context has been stopped, update return value as per collection
        // status(0 for collection done, 1 for collection not done)
        l_rc = !m_collectionDone;
    }
    catch (const std::exception& l_ex)
    {
        if (!m_ioContext.stopped())
        {
            m_ioContext.stop();
        }

        m_logger->logMessage(
            "Failed to trigger all FRU VPD collection. Error: " +
            std::string(l_ex.what()));

        throw l_ex;
    }
    return l_rc;
}

void FruVpdCollectionManager::registerVpdCollectionStatusListener() noexcept
{
    try
    {
        static std::shared_ptr<sdbusplus::bus::match_t>
            l_collectionStatusMatch = std::make_shared<sdbusplus::bus::match_t>(
                *m_asioConn,
                sdbusplus::bus::match::rules::propertiesChanged(
                    OBJPATH, vpd::constants::vpdCollectionInterface),
                [this](sdbusplus::message_t& l_msg) {
                    vpdCollectionStatusCallback(l_msg);
                });
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to register listener on FRU VPD collection status property. Error: " +
            std::string(l_ex.what()));
    }
}

void FruVpdCollectionManager::vpdCollectionStatusCallback(
    sdbusplus::message_t& i_msg) noexcept
{
    try
    {
        if (i_msg.is_method_error())
        {
            m_logger->logMessage(
                "Error in reading VPD collection status signal. ");
            return;
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
                m_logger->logMessage("VPD collection is completed");

                // set collection done flag
                m_collectionDone = true;

                // cancel the timer
                m_collectionTimer->cancel();

                // stop the event loop
                m_ioContext.stop();
            }
            else
            {
                m_logger->logMessage("VPD collection not yet complete");
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to process VPD collection status callback. Error : " +
            std::string(l_ex.what()));
    }
}

void FruVpdCollectionManager::handleTimerExpiry(
    const boost::system::error_code& i_errorCode) noexcept
{
    try
    {
        if (i_errorCode == boost::asio::error::operation_aborted)
        {
            m_logger->logMessage("Timer aborted for VPD collection status");
            return;
        }

        if (i_errorCode)
        {
            m_logger->logMessage("Timer failed for VPD collection status" +
                                 std::string(i_errorCode.message()));
            return;
        }
        m_logger->logMessage(
            "VPD collection not done even after timer expiry of " +
            std::format("{:%T}", m_collectionStatusTimeout) + "s");

        // cancel the timer
        m_collectionTimer->cancel();

        // stop the I/O context loop
        m_ioContext.stop();
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to handle collection timer expiry. Error : " +
            std::string(l_ex.what()));
    }
}
