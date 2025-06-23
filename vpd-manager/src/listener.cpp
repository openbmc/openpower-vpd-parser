#include "listener.hpp"

#include "event_logger.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "utility/json_utility.hpp"

namespace vpd
{
Listener::Listener(
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection) :
    m_asioConnection(i_asioConnection)
{}

void Listener::registerAllCorrPropCallBack(
    [[maybe_unused]] const std::string& i_correlatedPropJsonPath) noexcept
{
    try
    {
        m_correlatedPropJson =
            jsonUtility::getParsedJson(i_correlatedPropJsonPath);
        if (m_correlatedPropJson.empty())
        {
            throw JsonException("Failed to parse correlated properties JSON",
                                i_correlatedPropJsonPath);
        }
        /* TODO:
        Parse correlated_properties JSON, and register callback for all
        interfaces under all services */
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void Listener::registerCorrPropCallBack(
    [[maybe_unused]] const std::string& i_service,
    [[maybe_unused]] const std::string& i_interface,
    [[maybe_unused]] std::function<void(sdbusplus::message_t& i_msg)>
        i_callBackFunction)
{
    try
    {
        /*TODO:
        Create match object based on service name, interface and callback
        function.
        */
    }
    catch (const std::exception& l_ex)
    {
        throw FirmwareException(l_ex.what());
    }
}

void Listener::correlatedPropChangedCallBack(
    sdbusplus::message_t& i_msg) noexcept
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
