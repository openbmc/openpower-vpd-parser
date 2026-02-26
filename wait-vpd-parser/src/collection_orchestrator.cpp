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
        m_ioContext.stop();

        m_logger->logMessage(std::format(
            "CollectionOrchestrator failed. Error: {}", l_ex.what()));

        throw;
    }
}

void CollectionOrchestrator::collectAllFruVpdCallback(
    [[maybe_unused]] boost::system::error_code i_ec,
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
        */
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
             - Update state member variable to Completed
             - Stop timer
             - Stop I/O context
        */
    }
    catch (const std::exception& l_ex)
    {
        throw std::runtime_error(std::format(
            "Failed to process VPD collection status callback. Error : {}",
            l_ex.what()));
    }
}
