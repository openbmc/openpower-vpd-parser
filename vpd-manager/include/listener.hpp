#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>

#include <map>
#include <memory>
#include <string>

namespace vpd
{
using MatchObject = std::map<
    std::string,
    std::map<std::string, std::shared_ptr<sdbusplus::asio::connection>>>;
using DbusData = std::set<std::tuple<std::string, std::string, std::string>>;

class Listener
{
  public:
    /**
     * @brief Constructs Listener objects
     */
    Listener(const std::shared_ptr<sdbusplus::asio::connection>);

    /**
     * @brief Register correlated properties change event
     *
     * This API parses through the JSON to get all the correlated interfaces and
     * registers dbus PropertiesChanged event on those interfaces. This API also
     * saves the pointers to registered sdbus match object in
     * m_listOfMatchObjects.
     */
    void registerAllCorrelatedDbusPropChange();

    /**
     * @brief Get sdbusplus match object
     *
     * This API searches the sdbusplus match object in m_listOfMatchObjects
     * based on the given service and interface and returns the sdbusplus match
     * object if present.
     *
     * @param[in] i_service - D-bus service name
     * @param[in] i_interface - D-bus interface
     *
     * @return Valid sdbusplus match object if present in m_listOfMatchObjects,
     * else return nullptr.
     */
    std::shared_ptr<sdbusplus::bus::match_t> getMatchObj(
        const std::string& i_service, const std::string& i_interface);

    /**
     * @brief De-register the sdbuplus match event.
     *
     * This API gets the match object for the given service and interface and
     * de-registers it by resetting it to nullptr and also removes the entry
     * from m_listOfMatchObjects.
     *
     * @param[in] i_service - Service name.
     * @param[in] i_interface -Interface name.
     */
    void deRegisterDbusEvent(const std::string& i_service,
                             const std::string& i_interface);

    /**
     * @brief Pause the callback response.
     *
     * This API saves the given service name, object path, interface into
     * m_pausedInvData, if not present.
     *
     * @param[in] i_serviceName - D-bus service name.
     * @param[in] i_objectPath - D-bus object path.
     * @param[in] i_interface - D-bus interface.
     *
     * @return 0 if entries are added successfully, 1 on failure.
     */
    int pauseResponse(const std::string& i_serviceName,
                      sdbusplus::message::object_path& i_objectPath,
                      const std::string& i_interface);

    /**
     * @brief Resume the callback response.
     *
     * This API deletes the given service name, object path , interface pair
     * from m_pausedInvData, if present.
     *
     * @param[in] i_serviceName - D-bus service name.
     * @param[in] i_objectPath - D-bus object path.
     * @param[in] i_interface - D-bus interface.
     *
     * @return 0 if entries are deleted successfully, 1 on failure.
     */
    int resumeResponse(const std::string& i_serviceName,
                       sdbusplus::message::object_path& i_objectPath,
                       const std::string& i_interface);

  private:
    /**
     * Callback function to handle correlated d-bus property change events
     *
     * @param[in] i_msg - Callback message received on an event
     */
    void handleCorrelatedDbusPropChange(sdbusplus::message_t& i_msg);

    /**
     * @brief Check if the response is paused
     *
     * This API checks if the given service name, object path, interface is
     * present in m_pausedInvData.
     *
     * @param[in] i_serviceName - Service name.
     * @param[in] i_objectPath - D-bus object path.
     * @param[in] i_interface - D-bus interface.
     *
     * @return true if the given d-bus data is present in m_pausedInvData, false
     * otherwise.
     */
    bool isResponsePaused(const std::string& i_serviceName,
                          sdbusplus::message::object_path& i_objectPath,
                          const std::string& i_interface);

    /** sdbusplus asio object */
    const std::shared_ptr<sdbusplus::asio::connection> m_asioConnection;

    /** List of registered match objects */
    MatchObject m_listOfMatchObjects;

    /** Set of inventory data, whose response has to be paused. */
    DbusData m_pausedInvData;
};
} // namespace vpd
