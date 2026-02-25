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
        m_state = State::Idle;

        constexpr uint64_t l_collectionTriggerTimeoutUs{
            1 * 60 * 1000000}; // 1 min
                               // timeout

        m_asioConn->async_method_call_timed(
            [this](boost::system::error_code l_ec,
                   sdbusplus::message::message& l_msg) {
                this->collectAllFruVpdCallback(l_ec, l_msg);
            },
            m_collectionServiceName, m_objectPath, m_interface,
            m_collectionMethodName, l_collectionTriggerTimeoutUs);

        // Collection has been triggered
        m_state = State::CollectionTriggered;

        // start the event loop with a timeout
        // collection trigger timeout value + collection status timeout value
        m_ioContext.run_for(boost::asio::chrono::seconds(
                                l_collectionTriggerTimeoutUs / 1000000) +
                            m_collectionStatusTimeout);

        // I/O context has been stopped, check state and provide detailed error
        // codes
        switch (m_state)
        {
            case State::CollectionTriggered:
            {
                throw std::runtime_error(
                    "Collection triggered but collection failed");
                break;
            }
            case State::CollectionTriggerTimeout:
            {
                throw std::runtime_error("Collection trigger timed out");
                break;
            }
            case State::CollectionCompleted:
            {
                m_logger->logMessage("VPD collection is complete");
                break;
            }
            case State::Listening:
            {
                // timer expired while listening, collection not done within
                // timeout, throw exception so that service fails
                throw std::runtime_error(
                    "Timed out listening for collection status");

                break;
            }

            default:
            case State::Failed:
            case State::Idle:
            {
                // collection failed, throw exception so that service fails
                throw std::runtime_error(
                    "Collection failed due to internal error");
                break;
            }
        };

        l_rc = static_cast<int>(m_state);
    }
    catch (const std::exception& l_ex)
    {
        m_ioContext.stop();

        m_logger->logMessage(
            std::format("Failed to trigger all FRU VPD collection. Error: {}",
                        l_ex.what()));

        m_state = State::Failed;

        throw l_ex;
    }
    return l_rc;
}

void FruVpdCollectionManager::collectAllFruVpdCallback(
    boost::system::error_code i_ec, sdbusplus::message_t& i_msg) noexcept
{
    try
    {
        m_logger->logMessage("_SR async_send callback");

        if (i_ec == boost::system::errc::timed_out)
        {
            m_state = State::CollectionTriggerTimeout;

            throw vpd::DbusException(
                "wait-vpd-parsers timed out trying to call \"CollectAllFRUVPD\"");
        }
        else if (i_ec || i_msg.is_method_error())
        {
            m_state = State::Failed;

            throw vpd::DbusException(
                "Error with \"CollectAllFRUVPD\" async_send" + i_ec.message());
        }
        else
        {
            bool l_collectionTriggered{false};

            // D-Bus call has returned
            i_msg.read(l_collectionTriggered);

            if (l_collectionTriggered)
            {
                m_state = State::CollectionTriggered;

                m_logger->logMessage("CollectAllFRUVPD successful");

                // start a timer to check if collection status is
                // completed within a fixed time
                m_collectionTimer = make_shared<boost::asio::steady_timer>(
                    m_ioContext, m_collectionStatusTimeout);

                m_collectionTimer->async_wait(
                    boost::bind(&FruVpdCollectionManager::handleTimerExpiry,
                                this, boost::asio::placeholders::error));

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
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Error processing collect all FRU VPD callback: {}", l_ex.what()));

        m_state = State::Failed;

        m_ioContext.stop();
    }
}

void FruVpdCollectionManager::registerVpdCollectionStatusListener() noexcept
{
    try
    {
        m_state = State::Listening;

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
        m_logger->logMessage(std::format(
            "Failed to register listener on FRU VPD collection status property. Error: {}",
            l_ex.what()));

        m_state = State::Failed;
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
                m_state = State::CollectionCompleted;

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
        m_logger->logMessage(std::format(
            "Failed to process VPD collection status callback. Error : {}",
            l_ex.what()));
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
            m_logger->logMessage(
                std::format("Timer failed for VPD collection status. Error: {}",
                            i_errorCode.message()));
            return;
        }
        m_logger->logMessage(std::format(
            "VPD collection not done even after timer expiry of {:%T}s",
            m_collectionStatusTimeout));

        // cancel the timer
        m_collectionTimer->cancel();

        // stop the I/O context loop
        m_ioContext.stop();
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Failed to handle collection timer expiry. Error : {}",
                        l_ex.what()));
    }
}
