#pragma once

#include "types.hpp"

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

#include <map>
#include <memory>
#include <string>

namespace vpd
{
/**
 * @brief Class to listen on events
 *
 * This class can be used to register and handle events on the system.
 */
class Listener
{
  public:
    Listener(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    ~Listener() = default;

    /**
     * @brief Constructs Listener objects
     *
     * @param[in] i_asioConn - D-bus object to perform asynchronous read/write
     * operations.
     */
    Listener(const std::shared_ptr<sdbusplus::asio::connection>& i_asioConn) :
        m_asioConnection(i_asioConn)
    {}

    /**
     * @brief Register correlated properties change event
     *
     * This API parses through the JSON to get all the correlated interfaces and
     * registers dbus PropertiesChanged event on those interfaces. This API also
     * saves the pointers to registered sdbus match object in
     * m_listOfMatchObjects.
     */
    void regAllCorrelatedDbusProps();

    /**
     * @brief Get sdbusplus match object
     *
     * This API searches the sdbusplus match object in m_listOfMatchObjects
     * based on the given service and interface and returns the sdbusplus match
     * object if present.
     *
     * @param[in] i_serviceName - D-bus service name
     * @param[in] i_interface - D-bus interface
     *
     * @return Valid sdbusplus match object if present in m_listOfMatchObjects,
     * else return nullptr.
     */
    std::shared_ptr<sdbusplus::bus::match_t> getMatchObj(
        const std::string& i_serviceName, const std::string& i_interface);

    /**
     * @brief De-register the sdbuplus match event.
     *
     * This API gets the match object for the given service and interface and
     * de-registers it by resetting it to nullptr and also removes the entry
     * from m_listOfMatchObjects. If the given service and interface doesn't
     * exist in m_listOfMatchObjects, no action is taken.
     *
     * @param[in] i_serviceName - Service name.
     * @param[in] i_interface - Interface name.
     *
     * @return 0 If either no action is performed or on successful
     * deregistration of match object, -1 on failure.
     */
    uint8_t deRegDbusEvent(const std::string& i_serviceName,
                           const std::string& i_interface);

    /**
     * @brief Pause the callback response.
     *
     * This API saves the given service name, object path, interface into
     * m_pausedInvData, if not present already.
     * The saved list will be used by the callback function to decide whether or
     * not the response action has to be paused.
     *
     * @param[in] i_serviceName - D-bus service name.
     * @param[in] i_objectPath - D-bus object path.
     * @param[in] i_interface - D-bus interface.
     *
     * @return 0 if entries are added successfully, -1 on failure.
     */
    uint8_t pauseResponse(const std::string& i_serviceName,
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
     * @return 0 if entries are deleted successfully, -1 on failure.
     */
    uint8_t resumeResponse(const std::string& i_serviceName,
                           sdbusplus::message::object_path& i_objectPath,
                           const std::string& i_interface);

    /**
     * @brief Pause all callbacks
     *
     * This API is required to pause all callbacks at once.
     */
    void pauseAllCallbacks();

  private:
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
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConnection;

    /** List of registered match objects */
    types::MatchObject m_listOfMatchObjects;

    /** Set of inventory data, whose response has to be paused. */
    types::DbusData m_pausedInvData;

    /** Parsed JSON object */
    nlohmann::json m_parsedCorrelatedJson;
};
} // namespace vpd
