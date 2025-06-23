#pragma once

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>

namespace vpd
{
/**
 * @brief Class to listen on events
 *
 * This class will be used for registering and handling events on the system.
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
     * @param[in] i_correlatedPropJsonPath - Path to correlated properties JSON
     * file.
     */
    Listener(
        const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection,
        const std::string& i_correlatedPropJsonPath);

    /**
     * @brief API to register call back for all interfaces hosting correlated
     * properties
     *
     * This API registers callback for all the interfaces in
     * correlated_properties.json
     *
     * @return On successful registration, returns true, otherwise returns
     * false.
     */
    bool registerCorrPropCallBack() const noexcept;

  private:
    // Shared pointer to bus connection.
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConnection;

    // Parsed correlated properties JSON.
    nlohmann::json m_correlatedPropJson{};

    /**
     * @brief API which is called when correlated property change is detected
     *
     */
    void propertiesChangedCallBack(
        const sdbusplus::message_t& i_msg) const noexcept;
};
} // namespace vpd
