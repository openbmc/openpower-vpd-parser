#pragma once

#include "constants.hpp"
#include "types.hpp"
#include "worker.hpp"

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
     * @param[in] i_worker - Reference to worker class object.
     * @param[in] i_asioConnection - Dbus Connection.
     *
     * @throw std::runtime_error
     */
    Listener(
        const std::shared_ptr<Worker>& i_worker,
        const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection);

    /**
     * @brief API to register callback for Host state change.
     *
     */
    void registerHostStateChangeCallback() const noexcept;

    /**
     * @brief API to register callback for "AssetTag" property change.
     */
    void registerAssetTagChangeCallback() const noexcept;

    /**
     * @brief API to register "Present" property change callback
     *
     * This API registers "Present" property change callback for FRUs for
     * which "monitorPresence" is true in system config JSON.
     */
    void registerPresenceChangeCallback() noexcept;

    /**
     * @brief API to register callback for all correlated properties.
     *
     * This API registers properties changed callback for all the interfaces in
     * given correlated properties JSON file.
     *
     * @param[in] i_correlatedPropJsonFile - File path of correlated properties
     * JSON.
     */
    void registerCorrPropCallBack(
        const std::string& i_correlatedPropJsonFile) noexcept;

    /**
     * @brief API to register properties changed callback.
     *
     * This API registers a properties changed callback for a specific interface
     * under a service by constructing a match object. This API also saves the
     * constructed match object into map object map data member.
     *
     * @param[in] i_service - Service name.
     * @param[in] i_interface - Interface name.
     * @param[in] i_callBackFunction - Callback function.
     *
     * @throw FirmwareException
     */
    void registerPropChangeCallBack(
        const std::string& i_service, const std::string& i_interface,
        std::function<void(sdbusplus::message_t& i_msg)> i_callBackFunction);

  private:
    /**
     * @brief API to process host state change callback.
     *
     * @param[in] i_msg - Callback message.
     */
    void hostStateChangeCallBack(sdbusplus::message_t& i_msg) const noexcept;

    /**
     * @brief Callback API to be triggered on "AssetTag" property change.
     *
     * @param[in] i_msg - Callback message.
     */
    void assetTagChangeCallback(sdbusplus::message_t& i_msg) const noexcept;

    /**
     * @brief Callback API to be triggered on "Present" property change.
     *
     * @param[in] i_msg - Callback message.
     */
    void presentPropertyChangeCallback(
        sdbusplus::message_t& i_msg) const noexcept;

    /**
     * @brief API which is called when correlated property change is detected
     *
     * @param[in] i_msg - Callback message.
     */
    void correlatedPropChangedCallBack(sdbusplus::message_t& i_msg) noexcept;

    /**
     * @brief API to get correlated properties for given property.
     *
     * For a given service name, object path, interface and property, this API
     * uses parsed correlated properties JSON object and returns a list of
     * correlated object path, interface and property. Correlated properties are
     * properties which are hosted under different interfaces with same or
     * different data type, but share the same data. Hence if the data of a
     * property is updated, then it's respective correlated property/properties
     * should also be updated so that they remain in sync.
     *
     * @param[in] i_serviceName - Service name.
     * @param[in] i_objectPath - Object path.
     * @param[in] i_interface - Interface name.
     * @param[in] i_property - Property name.
     *
     * @return On success, returns a vector of correlated object path, interface
     * and property. Otherwise returns an empty vector.
     *
     * @throw FirmwareException
     */
    types::DbusPropertyList getCorrelatedProps(
        const std::string& i_serviceName, const std::string& i_objectPath,
        const std::string& i_interface, const std::string& i_property) const;

    /**
     * @brief API to update a given correlated property
     *
     * This API updates a given correlated property on Dbus. For updates to
     * properties on Phosphor Inventory Manager it uses Phosphor Inventory
     * Manager's "Notify" API to update the given property.
     *
     * @param[in] i_serviceName - Service name.
     * @param[in] i_corrProperty - Details of correlated property to update
     * @param[in] i_value - Property value
     *
     * @return true, if correlated property was successfully updated, false
     * otherwise.
     */
    bool updateCorrelatedProperty(
        const std::string& i_serviceName,
        const types::DbusPropertyEntry& i_corrProperty,
        const types::DbusVariantType& i_value) const noexcept;

    // Shared pointer to worker class
    const std::shared_ptr<Worker>& m_worker;

    // Shared pointer to bus connection.
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConnection;

    // Map of inventory path to Present property match object
    types::FruPresenceMatchObjectMap m_fruPresenceMatchObjectMap;

    // Parsed correlated properties JSON.
    nlohmann::json m_correlatedPropJson{};

    // A map of {service name,{interface name,match object}}
    types::MatchObjectMap m_matchObjectMap;
};
} // namespace vpd
