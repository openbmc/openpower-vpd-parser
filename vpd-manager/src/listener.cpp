#include "listener.hpp"

#include "event_logger.hpp"
#include "exceptions.hpp"
#include "logger.hpp"

namespace vpd
{
Listener::Listener(
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection) :
    m_asioConnection(i_asioConnection)
{}

void Listener::registerCorrPropCallBack(
    [[maybe_unused]] const std::string& i_correlatedPropJsonPath) noexcept
{
    try
    {
        /* TODO:
        Parse correlated_properties JSON, and register callback for all
        interfaces */
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::correlatedPropChangedCallBack(
    const sdbusplus::message_t& i_msg) const noexcept
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw DbusException("Error in reading property change signal.");
            return;
        }

        /*TODO:
        1. Extract interface, object path and property name from the message
        2. Use correlated JSON to find target {object path, interface,
        property/properties} to update*/
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}
} // namespace vpd
