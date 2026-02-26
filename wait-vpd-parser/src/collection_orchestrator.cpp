#include "config.h"

#include "collection_orchestrator.hpp"

#include "exceptions.hpp"

#include <format>

int CollectionOrchestrator::triggerFruVpdCollectionAndCheckStatus()
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

void CollectionOrchestrator::collectAllFruVpdCallback(
    boost::system::error_code i_ec, sdbusplus::message_t& i_msg) noexcept
{
    try
    {
        if (i_ec == boost::system::errc::timed_out)
        {
            m_state = State::CollectionTriggerTimeout;

            // stop the I/O context
            m_ioContext.stop();
            return;
        }
        else if (i_ec || i_msg.is_method_error())
        {
            m_state = State::Failed;
            // stop the I/O context
            m_ioContext.stop();
            return;
        }

        bool l_collectionTriggered{false};

        // D-Bus call has returned, read the return value
        i_msg.read(l_collectionTriggered);

        if (!l_collectionTriggered)
        {
            // collect FRU VPD D-Bus call has returned false
            m_state = State::Failed;
            // stop the I/O context
            m_ioContext.stop();
            return;
        }

        m_state = State::CollectionTriggered;

        m_logger->logMessage("CollectAllFRUVPD successful");

        // triggering FRU VPD collection is successful, now
        // start a Listener on the Collection Status
        // property
        registerVpdCollectionStatusListener();
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Error in collect all FRU VPD callback: {}", l_ex.what()));

        m_state = State::Failed;

        m_ioContext.stop();
    }
}

void CollectionOrchestrator::registerVpdCollectionStatusListener() noexcept
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

        m_state = State::Listening;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to register listener on FRU VPD collection status property. Error: {}",
            l_ex.what()));

        m_state = State::Failed;
    }
}

void CollectionOrchestrator::vpdCollectionStatusCallback(
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
