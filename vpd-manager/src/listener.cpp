#include "listener.hpp"

#include "logger.hpp"
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
        1. parse correlated_properties JSON
        2. */
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

    std::string l_objPath;
    i_msg.read(l_objPath);
    logging::logMessage("Property change detected on " + l_objPath);
}
} // namespace vpd
