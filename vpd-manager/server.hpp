#pragma once
#include <systemd/sd-bus.h>

#include <map>
#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/server.hpp>
#include <string>
#include <tuple>
#include <variant>

namespace sdbusplus
{
namespace com
{
namespace ibm
{
namespace VPD
{
namespace server
{

class Manager
{
  public:
    /* Define all of the basic class operations:
     *     Not allowed:
     *         - Default constructor to avoid nullptrs.
     *         - Copy operations due to internal unique_ptr.
     *         - Move operations due to 'this' being registered as the
     *           'context' with sdbus.
     *     Allowed:
     *         - Destructor.
     */
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;
    virtual ~Manager() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    Manager(bus::bus& bus, const char* path);

    /** @brief Implementation for WriteKeyword
     *  A method to update the keyword value for a given VPD record.
     *
     *  @param[in] path - Path to the D-Bus object that represents the FRU.
     *  @param[in] record - Record whose keyword value needs to be modified.
     *  @param[in] keyword - Keyword whose value needs to be updated.
     *  @param[in] value - Value to be updated for the keyword.
     */
    virtual void writeKeyword(sdbusplus::message::object_path path,
                              std::string record, std::string keyword,
                              std::vector<uint8_t> value) = 0;

    /** @brief Implementation for GetFRUsByUnexpandedLocationCode
     *  A method to get list of FRU D-BUS object paths for a given unexpanded
     * location code.
     *
     *  @param[in] locationCode - An un-expanded Location code.
     *  @param[in] nodeNumber - Denotes the node in case of a multi-node
     * configuration, ignored on a single node system.
     *
     *  @return inventoryList[std::vector<sdbusplus::message::object_path>] -
     * List of all the FRUs D-Bus object paths for the given location code.
     */
    virtual std::vector<sdbusplus::message::object_path>
        getFRUsByUnexpandedLocationCode(std::string locationCode,
                                        uint16_t nodeNumber) = 0;

    /** @brief Implementation for GetFRUsByExpandedLocationCode
     *  A method to get list of FRU D-BUS object paths for a given expanded
     * location code.
     *
     *  @param[in] locationCode - Location code in expanded format.
     *
     *  @return inventoryList[std::vector<sdbusplus::message::object_path>] -
     * List of all the FRUs D-Bus object path for the given location code.
     */
    virtual std::vector<sdbusplus::message::object_path>
        getFRUsByExpandedLocationCode(std::string locationCode) = 0;

    /** @brief Implementation for GetExpandedLocationCode
     *  An api to get expanded location code corresponding to a given
     * un-expanded location code. Expanded location codes gives the location of
     * the FRU in the system.
     *
     *  @param[in] locationCode - Location code in un-expanded format.
     *  @param[in] nodeNumber - Denotes the node in case of multi-node
     * configuration. Ignored in case of single node configuration.
     *
     *  @return locationCode[std::string] - Location code in expanded format.
     */
    virtual std::string getExpandedLocationCode(std::string locationCode,
                                                uint16_t nodeNumber) = 0;

    /** @brief Implementation for FixBrokenEcc
     *  An api which fixes the broken ecc for the given object path.
     *
     *  @param[in] path - Path to the D-Bus object that represents the FRU.
     */
    virtual void fixBrokenEcc(sdbusplus::message::object_path path) = 0;

    /** @brief Emit interface added */
    void emit_added()
    {
        _com_ibm_VPD_Manager_interface.emit_added();
    }

    /** @brief Emit interface removed */
    void emit_removed()
    {
        _com_ibm_VPD_Manager_interface.emit_removed();
    }

    static constexpr auto interface = "com.ibm.VPD.Manager";

  private:
    /** @brief sd-bus callback for WriteKeyword
     */
    static int _callback_WriteKeyword(sd_bus_message*, void*, sd_bus_error*);

    /** @brief sd-bus callback for GetFRUsByUnexpandedLocationCode
     */
    static int _callback_GetFRUsByUnexpandedLocationCode(sd_bus_message*, void*,
                                                         sd_bus_error*);

    /** @brief sd-bus callback for GetFRUsByExpandedLocationCode
     */
    static int _callback_GetFRUsByExpandedLocationCode(sd_bus_message*, void*,
                                                       sd_bus_error*);

    /** @brief sd-bus callback for GetExpandedLocationCode
     */
    static int _callback_GetExpandedLocationCode(sd_bus_message*, void*,
                                                 sd_bus_error*);

    /** @brief sd-bus callback for FixBrokenEcc
     */
    static int _callback_FixBrokenEcc(sd_bus_message*, void*, sd_bus_error*);

    static const vtable::vtable_t _vtable[];
    sdbusplus::server::interface::interface _com_ibm_VPD_Manager_interface;
    sdbusplus::SdBusInterface* _intf;
};

} // namespace server
} // namespace VPD
} // namespace ibm
} // namespace com

namespace message
{
namespace details
{
} // namespace details
} // namespace message
} // namespace sdbusplus
