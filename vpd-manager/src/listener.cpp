#include "listener.hpp"

namespace vpd
{
    Listener::Listener(const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection): m_asioConnection(i_asioConnection)
    {}
}