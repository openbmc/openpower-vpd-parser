#pragma once

#include "logger.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>

#include <string>

/**
 * @brief Class to prime system blueprint.
 *
 * This class will be used for priming the system, by traversing the system
 * config JSON and primes all the FRU paths which qualifies for priming and
 * publishes inventory object paths on the DBus.
 */
class PrimeInventory
{
  public:
    /**
     * Deleted methods
     */
    PrimeInventory(const PrimeInventory&) = delete;
    PrimeInventory& operator=(const PrimeInventory&) = delete;
    PrimeInventory& operator=(PrimeInventory&&) = delete;
    PrimeInventory(PrimeInventory&&) = delete;

    /**
     * Contructor
     *
     * @throw std::exception
     */
    PrimeInventory();

    /**
     * @brief API to prime system blueprint.
     *
     * The API will traverse the system config JSON and will prime all the FRU
     * paths which qualifies for priming.
     *
     */
    void primeSystemBlueprint() const noexcept;

  private:
    /**
     * @brief API to check if priming is required.
     *
     * The API will traverse the system config JSON and counts the FRU
     * paths which qualifies for priming and compares with count of object paths
     * found under PIM which hosts the "xyz.openbmc_project.Common.Progress"
     * interface. If the dbus count is equal to or greater than the count from
     * JSON config consider as priming is not required.
     *
     * @return true if priming is required, false otherwise.
     */
    bool isPrimingRequired() const noexcept;

    /**
     * @brief API to prime inventory Objects.
     *
     * @param[out] o_objectInterfaceMap - Interface and its properties map.
     * @param[in] i_fruJsonObj - FRU json object.
     *
     * @return true if the prime inventory is success, false otherwise.
     */
    bool primeInventory(vpd::types::ObjectMap& o_objectInterfaceMap,
                        const nlohmann::json& i_fruJsonObj) const noexcept;

    /**
     * @brief API to populate all required interface for a FRU.
     *
     * @param[in] i_interfaceJson - JSON containing interfaces to be populated.
     * @param[in,out] io_interfaceMap - Map to hold populated interfaces.
     * @param[in] i_parsedVpdMap - Parsed VPD as a map.
     */
    void populateInterfaces(
        const nlohmann::json& i_interfaceJson,
        vpd::types::InterfaceMap& io_interfaceMap,
        const vpd::types::VPDMapVariant& i_parsedVpdMap) const noexcept;

    /**
     * @brief API to check if present property should be handled for given FRU.
     *
     * vpd-manager should update present property for a FRU if and only if it's
     * not synthesized and vpd-manager handles present property for the FRU.
     * This API assumes "handlePresence" tag is a subset of "synthesized" tag.
     *
     * @param[in] i_fru -  JSON block for a single FRU.
     *
     * @return true if present property should be handled, false otherwise.
     */
    inline bool isPresentPropertyHandlingRequired(
        const nlohmann::json& i_fru) const noexcept
    {
        return !i_fru.value("synthesized", false) &&
               i_fru.value("handlePresence", true);
    }

    /**
     * @brief API to update "Functional" property.
     *
     * The API sets the default value for "Functional" property once if the
     * property is not yet populated over DBus. As the property value is not
     * controlled by the VPD-Collection process, if it is found already
     * populated, the functions skips re-populating the property so that already
     * existing value can be retained.
     *
     * @param[in] i_inventoryObjPath - Inventory path as read from config JSON.
     * @param[in,out] io_interfaces - Map to hold all the interfaces for the
     * FRU.
     */
    void processFunctionalProperty(
        const std::string& i_inventoryObjPath,
        vpd::types::InterfaceMap& io_interfaces) const noexcept;

    /**
     * @brief API to update "enabled" property.
     *
     * The API sets the default value for "enabled" property once if the
     * property is not yet populated over DBus. As the property value is not
     * controlled by the VPD-Collection process, if it is found already
     * populated, the functions skips re-populating the property so that already
     * existing value can be retained.
     *
     * @param[in] i_inventoryObjPath - Inventory path as read from config JSON.
     * @param[in,out] io_interfaces - Map to hold all the interfaces for the
     * FRU.
     */
    void processEnabledProperty(
        const std::string& i_inventoryObjPath,
        vpd::types::InterfaceMap& io_interfaces) const noexcept;

    /**
     * @brief API to update "available" property.
     *
     * The API sets the default value for "available" property once if the
     * property is not yet populated over DBus. On subsequent boot/reboot
     * if it is found already populated, the functions skips re-populating
     * the property so that already existing value can be retained.
     *
     * @param[in] i_inventoryObjPath - Inventory path as read from config JSON.
     * @param[in,out] io_interfaces - Map to hold all the interfaces for the
     * FRU.
     */
    void processAvailableProperty(
        const std::string& i_inventoryObjPath,
        vpd::types::InterfaceMap& io_interfaces) const noexcept;

    // Parsed JSON file.
    nlohmann::json m_sysCfgJsonObj{};

    std::shared_ptr<vpd::Logger> m_logger;
};
