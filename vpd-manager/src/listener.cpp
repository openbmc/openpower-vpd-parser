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
    const std::string& i_correlatedPropJsonPath) noexcept
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

        const nlohmann::json& l_serviceObj =
            m_correlatedPropJson.get_ref<const nlohmann::json::object_t&>();

        // Iterate through all services in the correlated properties json
        for (const auto& l_service : l_serviceObj.items())
        {
            const auto& l_serviceName = l_service.key();

            const nlohmann::json& l_invServiceObj =
                m_correlatedPropJson[l_serviceName]
                    .get_ref<const nlohmann::json::object_t&>();

            // register properties changed D-Bus signal callback
            // for all interfaces under this service.
            std::for_each(l_invServiceObj.items().begin(),
                          l_invServiceObj.items().end(),
                          [this, &l_serviceName = std::as_const(l_serviceName)](
                              const auto& i_invServiceObj) {
                              registerCorrPropCallBack(
                                  l_serviceName, i_invServiceObj.key(),
                                  correlatedPropChangedCallBack);
                          });
        } // service loop
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
    const std::string& i_service, const std::string& i_interface,
    std::function<void(sdbusplus::message_t& i_msg)> i_callBackFunction)
{
    try
    {
        types::MatchObjectPtr l_matchObj =
            std::make_unique<sdbusplus::bus::match::match>(
                static_cast<sdbusplus::bus_t&>(*m_asioConnection),
                "type='signal',member='PropertiesChanged',"
                "interface='org.freedesktop.DBus.Properties',"
                "arg0='" +
                    i_interface + "'",
                i_callBackFunction);

        // save the match object in map
        m_matchObjectMap[i_service][i_interface] = l_matchObj;
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
        }

        const std::string l_interface{i_msg.get_interface()};
        const std::string l_objectPath{i_msg.get_path()};
        const std::string l_signature{i_msg.get_signature()};

        (void)l_interface;
        (void)l_objectPath;
        (void)l_signature;

        /*TODO:
        Use correlated JSON to find target {object path, interface,
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
