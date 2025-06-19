#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

namespace vpd
{
/**
 * @brief Class to listen on events
 *
 * This class used for registering and handling events on the system.
 */
class Listener
{
    public:
    /**
     * Deleted methods for Listener
     */
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    Listener(Listener&&) = delete;

    /**
     * @brief Constructor
     *
     * @param[in] i_asioConnection - Dbus Connection.
     */
    Listener(const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection);

    private:
    // Shared pointer to bus connection.
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConnection;
};
}

