#include "listener.hpp"

namespace vpd
{
Listener::Listener(
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection) :
    m_asioConnection(i_asioConnection)
{}

bool Listener::registerCallBack() const noexcept
{
    bool l_rc{true};
    try
    {
        /* TODO:
        Parse correlated_properties JSON, and register callback for all
        interfaces */
    }
    catch (const std::exception& l_ex)
    {
        l_rc = false;
        logging::logMessage(
            "Failed to register call back for correlated interfaces. Error : " +
            std::string(l_ex.what()));
    }

    return l_rc;
}

void Listener::propertiesChangedCallBack(const sdbusplus::message_t& i_msg)
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
