#pragma once

#include "types.hpp"

#include <stdint.h>

#include <string>

namespace sdbusplus
{
namespace bus
{
class bus;
} // namespace bus
} // namespace sdbusplus

namespace sdbusplus
{
namespace message
{
class message;
} // namespace message
} // namespace sdbusplus

namespace openpower
{
namespace vpd
{
namespace manager
{

class Manager;
/**
 * @brief A class that handles changes to BIOS attributes backed by VPD.
 *
 * This class has APIs that handle updates to BIOS attributes that need to
 * be backed up to VPD. It mainly does the following:
 * 1) Checks if the VPD keywords that BIOS attributes are backed to are
 * uninitialized. If so, it initializes them.
 * 2) Listens for changes to BIOS attributes and synchronizes them to the
 * appropriate VPD keyword.
 *
 * Since on a factory reset like scenario, the BIOS attributes are initialized
 * by PLDM, this code waits until PLDM has grabbed a bus name before attempting
 * any syncs.
 */
class BiosHandler
{
  public:
    // Some default and deleted constructors and assignments.
    BiosHandler() = delete;
    BiosHandler(const BiosHandler&) = delete;
    BiosHandler& operator=(const BiosHandler&) = delete;
    BiosHandler(Manager&&) = delete;
    BiosHandler& operator=(BiosHandler&&) = delete;
    ~BiosHandler() = default;

    BiosHandler(sdbusplus::bus_t& bus, Manager& manager) :
        bus(bus), manager(manager)
    {
        checkAndListenPLDMService();
    }

  private:
    /**
     * @brief Check if PLDM service is running and run BIOS sync
     *
     * This API checks if the PLDM service is running and if yes it will start
     * an immediate sync of BIOS attributes. If the service is not running, it
     * registers a listener to be notified when the service starts so that a
     * restore can be performed.
     */
    void checkAndListenPLDMService();

    /**
     * @brief Register listener for changes to BIOS Attributes.
     *
     * The VPD manager needs to listen to changes to certain BIOS attributes
     * that are backed by VPD. When the attributes we are interested in
     * change, the VPD manager will make sure that we write them back to the
     * VPD keywords that back them up.
     */
    void listenBiosAttribs();

    /**
     * @brief Callback for BIOS Attribute changes
     *
     * Checks if the BIOS attribute(s) changed are those backed up by VPD. If
     * yes, it will update the VPD with the new attribute value.
     * @param[in] msg - The callback message.
     */
    void biosAttribsCallback(sdbusplus::message_t& msg);

    /**
     * @brief Save BIOS attributes to system backplane VPD
     *
     * Attributes from attribute table are updated on VPD with the value taken
     * from BIOS manager.
     *
     * @param[in] attrName - attribute name
     * @param[in] attrValInBios - The attribute's value in BIOS
     * @param[in] attrValInVPD  - The attribute's value in VPD
     */
    void saveBiosAttrToVPD(const BIOSAttribute& attrName,
                           const BIOSAttrValueType& attrValInBios,
                           const std::string& attrValInVPD);
    /**
     * @brief Update the BIOS attributes based on its corresponding VPD value
     *
     * Attributes from attribute table are updated to BIOS manager with the
     * value taken from VPD.
     *
     * @param[in] attrName - attribute name
     * @param[in] attrVpdVal - The attribute value as read from VPD.
     * @param[in] attrInBIOS - The attribute's value in the BIOS table.
     */
    void saveAttrToBIOS(const BIOSAttribute& attrName,
                        const std::string& attrVpdVal,
                        const BIOSAttrValueType& attrInBIOS);

    /**
     * @brief Restore BIOS attributes
     *
     * This function checks if we are coming out of a factory reset. If yes,
     * it checks the VPD cache for valid backed up copy of the applicable
     * BIOS attributes. If valid values are found in the VPD, it will apply
     * those to the BIOS attributes.
     */
    void restoreBIOSAttribs();

    /**
     * @brief Reference to the bus.
     */
    sdbusplus::bus_t& bus;

    /**
     * @brief Reference to the manager.
     */
    Manager& manager;

    /**
     * @brief BIOS attribute table
     * A table which maps attributes to its corresponding VPD record-keyword and
     * bitmask if its required to access specific bit. If an attribute doesn't
     * hold any bitmask, then 0x0 is stored as dummy bitmask value in the table.
     */
    BIOSAttributeTable attributeTable = {
        {"hb_field_core_override", std::make_tuple("VSYS", "RG", 0x0)},
        {"hb_memory_mirror_mode", std::make_tuple("UTIL", "D0", 0x0)},
        {"pvm_keep_and_clear", std::make_tuple("UTIL", "D1", 0x01)},
        {"pvm_create_default_lpar", std::make_tuple("UTIL", "D1", 0x02)},
        {"pvm_clear_nvram", std::make_tuple("UTIL", "D1", 0x04)}};

}; // class BiosHandler
} // namespace manager
} // namespace vpd
} // namespace openpower