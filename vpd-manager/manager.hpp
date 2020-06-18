#pragma once

#include "editor_impl.hpp"
#include "types.hpp"

#include <com/ibm/VPD/Manager/server.hpp>
#include <map>
#include <nlohmann/json.hpp>

namespace sdbusplus
{
namespace bus
{
class bus;
}
} // namespace sdbusplus

namespace openpower
{
namespace vpd
{
namespace manager
{

template <typename T>
using ServerObject = T;

using ManagerIface = sdbusplus::com::ibm::VPD::server::Manager;

/** @class Manager
 *  @brief OpenBMC VPD Manager implementation.
 *
 *  A concrete implementation for the
 *  com.ibm.vpd.Manager
 */
class Manager : public ServerObject<ManagerIface>
{
  public:
    /* Define all of the basic class operations:
     * Not allowed:
     * - Default constructor to avoid nullptrs.
     * - Copy operations due to internal unique_ptr.
     * - Move operations due to 'this' being registered as the
     *  'context' with sdbus.
     * Allowed:
     * - Destructor.
     */
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    ~Manager() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus connection.
     *  @param[in] busName - Name to be requested on Bus
     *  @param[in] objPath - Path to attach at.
     *  @param[in] iFace - interface to implement
     */
    Manager(sdbusplus::bus::bus&& bus, const char* busName, const char* objPath,
            const char* iFace);

    /** @brief Implementation for WriteKeyword
     *  Api to update the keyword value for a given inventory.
     *
     *  @param[in] path - Path to the D-Bus object that represents the FRU.
     *  @param[in] recordName - name of the record for which the keyword value
     *  has to be modified
     *  @param[in] keyword - keyword whose value needs to be updated
     *  @param[in] value - value that needs to be updated
     */
    void writeKeyword(const sdbusplus::message::object_path path,
                      const std::string recordName, const std::string keyword,
                      const Binary value);

    /** @brief Implementation for GetFRUsByUnexpandedLocationCode
     *  A method to get list of FRU D-BUS object paths for a given unexpanded
     *  location code. Returns empty vector if no FRU found for that location
     *  code.
     *
     *  @param[in] locationCode - An un-expanded Location code.
     *  @param[in] nodeNumber - Denotes the node in case of a multi-node
     *  configuration, ignored on a single node system.
     *
     *  @return inventoryList[std::vector<sdbusplus::message::object_path>] -
     *  List of all the FRUs D-Bus object paths for the given location code.
     */
    inventory::ListOfPaths
        getFRUsByUnexpandedLocationCode(const std::string locationCode,
                                        const uint16_t nodeNumber);

    /** @brief Implementation for GetFRUsByExpandedLocationCode
     *  A method to get list of FRU D-BUS object paths for a given expanded
     *  location code. Returns empty vector if no FRU found for that location
     *  code.
     *
     *  @param[in] locationCode - Location code in expanded format.
     *
     *  @return inventoryList[std::vector<sdbusplus::message::object_path>] -
     *  List of all the FRUs D-Bus object path for the given location code.
     */
    inventory::ListOfPaths
        getFRUsByExpandedLocationCode(const std::string locationCode);

    /** @brief Implementation for GetExpandedLocationCode
     *  An API to get expanded location code corresponding to a given
     *  un-expanded location code.
     *
     *  @param[in] locationCode - Location code in un-expaned format.
     *  @param[in] nodeNumber - Denotes the node in case of multi-node
     *  configuration. Ignored in case of single node configuration.
     *
     *  @return locationCode[std::string] - Location code in expanded format.
     */
    std::string getExpandedLocationCode(const std::string locationCode,
                                        const uint16_t nodeNumber);

    /** @brief Start processing DBus messages. */
    void run();

  private:
    /** @brief process the given JSON file
     */
    void processJSON();

    /** @brief Persistent sdbusplus DBus bus connection. */
    sdbusplus::bus::bus _bus;

    /** @brief sdbusplus org.freedesktop.DBus.ObjectManager reference. */
    sdbusplus::server::manager::manager _manager;

    // file to store parsed json
    nlohmann::json jsonFile;

    // map to hold mapping to inventory path to vpd file path
    // we need as map here as it is in reverse order to that of json
    inventory::FrusMap frus;

    // map to hold the mapping of location code and inventory path
    inventory::LocationCodeMap fruLocationCode;
};

} // namespace manager
} // namespace vpd
} // namespace openpower
