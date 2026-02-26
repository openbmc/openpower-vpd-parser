#include "config.h"

#include "collection_orchestrator.hpp"

#include <format>

int CollectionOrchestrator::triggerFruVpdCollectionAndCheckStatus()
{
    int l_rc{vpd::constants::VALUE_1};
    try
    {
        /*TODO:
            - Do timer based async D-Bus call with callback to trigger FRU VPD
           collection
            - Run I/O context loop with a timeout (blocking call)
            - After I/O context loop exits, check for error encountered, timer
           expiry
            - Incase of error encountered, timer expiry, etc, throw exception so
           that service fails
            - Else return success
        */
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
    [[maybe_unused]] boost::system::error_code i_ec,
    [[maybe_unused]] sdbusplus::message_t& i_msg) noexcept
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
        m_logger->logMessage(std::format(
            "Error processing collect all FRU VPD callback: {}", l_ex.what()));

        m_state = State::Failed;

        m_ioContext.stop();
    }
}

void CollectionOrchestrator::registerVpdCollectionStatusListener() noexcept
{
    try
    {
        m_state = State::Listening;

        /* TODO:
         - Register a listener on collection status property
        */
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
    [[maybe_unused]] sdbusplus::message_t& i_msg) noexcept
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
        m_logger->logMessage(std::format(
            "Failed to process VPD collection status callback. Error : {}",
            l_ex.what()));
    }
}

void CollectionOrchestrator::handleTimerExpiry(
    [[maybe_unused]] const boost::system::error_code& i_errorCode) noexcept
{
    try
    {
        /* TODO:
        - check error code to see why timer expired
        - update state member variable accordingly
        - stop I/O context loop
        */
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Failed to handle collection timer expiry. Error : {}",
                        l_ex.what()));
    }
}
