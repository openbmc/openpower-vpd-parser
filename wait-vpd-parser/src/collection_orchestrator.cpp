#include "config.h"

#include "collection_orchestrator.hpp"

#include <format>

void CollectionOrchestrator::triggerFruVpdCollectionAndCheckStatus()
{
    try
    {
        /*TODO:
            - Do timed async D-Bus call with callback to trigger FRU VPD
           collection
            - Run I/O context loop with a timeout (blocking call)
            - After I/O context loop exits, check if collection status is
           completed
                - if yes, return
            - Incase of error encountered, timer expiry, etc, throw exception so
           that service fails
        */
    }
    catch (const std::exception& l_ex)
    {
        std::call_once(m_stopOnceFlag, [this]() { m_ioContext.stop(); });

        m_logger->logMessage(std::format(
            "CollectionOrchestrator failed. Error: {}", l_ex.what()));

        throw;
    }
}

void CollectionOrchestrator::collectAllFruVpdCallback(
    [[maybe_unused]] const boost::system::error_code i_ec,
    [[maybe_unused]] sdbusplus::message_t& i_msg)
{
    try
    {
        /* TODO:
            - Check for timeout or error
            - If no error or timeout
                - if message indicates collection was triggered successfully
                    -  start a timer based listener on collection status
                        property
                    - after starting the listener, read the collection status
           property from D-Bus and if it is done, return early
        */
    }
    catch (const std::exception& l_ex)
    {
        std::call_once(m_stopOnceFlag, [this]() { m_ioContext.stop(); });

        throw std::runtime_error(std::format(
            "Error processing collect all FRU VPD callback: {}", l_ex.what()));
    }
}

void CollectionOrchestrator::registerVpdCollectionStatusListener()
{
    try
    {
        /* TODO:
         - Register a listener on collection status property
        */
    }
    catch (const std::exception& l_ex)
    {
        throw std::runtime_error(std::format(
            "Failed to register listener on FRU VPD collection status property. Error: {}",
            l_ex.what()));
    }
}

void CollectionOrchestrator::vpdCollectionStatusCallback(
    [[maybe_unused]] sdbusplus::message_t& i_msg)
{
    try
    {
        /* TODO:
            - Process the message to check if collection status is complete
            - If yes,
             - Update collection done flag to true
             - Stop I/O context
        */
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
        /* TODO:
            - Read the Collection status property from D-Bus to check if it is
           complete
            - If yes,
             - Update collection done flag to true
             - Stop I/O context
        */
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to read VPD collection status. Error : {}", l_ex.what()));
    }
}
