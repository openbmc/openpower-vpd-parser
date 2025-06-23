#include "listener.hpp"

#include "event_logger.hpp"
#include "logger.hpp"
#include "utility/json_utility.hpp"

namespace vpd
{
Listener::Listener(
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection,
    const std::string& i_correlatedPropJsonPath) :
    m_asioConnection(i_asioConnection)
{
    m_correlatedPropJson = jsonUtility::getParsedJson(i_correlatedPropJsonPath);
    if (m_correlatedPropJson.empty())
    {
        logging::logMessage("Failed to parse correlated properties JSON");
    }
}

void Listener::registerCorrPropCallBack() const noexcept
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
    if (i_msg.is_method_error())
    {
        logging::logMessage("Error in reading property change signal.");
        return;
    }

    /*TODO:
    1. Extract interface, object path and property name from the message
    2. Use correlated JSON to find target {object path, interface,
    property/properties} to update*/
}
} // namespace vpd
