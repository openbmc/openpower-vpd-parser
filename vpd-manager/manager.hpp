#pragma once

#include "bios_handler.hpp"
#include "editor_impl.hpp"
#include "gpioMonitor.hpp"

#include <map>
#include <sdbusplus/asio/object_server.hpp>

namespace openpower
{
namespace vpd
{
namespace manager
{

/** @class Manager
 *  @brief OpenBMC VPD Manager implementation.
 *
 *  Implements methods under interface com.ibm.vpd.Manager.
 */
class Manager
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
    ~Manager()
    {
        sd_bus_unref(sdBus);
    }

    /** @brief Constructor.
     *  @param[in] ioCon - IO context.
     *  @param[in] iFace - interface to implement.
     *  @param[in] connection - Dbus Connection.
     */
    Manager(std::shared_ptr<boost::asio::io_context>& ioCon,
            std::shared_ptr<sdbusplus::asio::dbus_interface>& iFace,
            std::shared_ptr<sdbusplus::asio::connection>& conn);

    /** @brief Implementation for WriteKeyword
     *  Api to update the keyword value for a given inventory.
     *
     *  @param[in] path - Path to the D-Bus object that represents the FRU.
     *  @param[in] recordName - name of the record for which the keyword value
     *  has to be modified
     *  @param[in] keyword - keyword whose value needs to be updated
     *  @param[in] value - value that needs to be updated
     */
    void writeKeyword(const sdbusplus::message::object_path& path,
                      const std::string& recordName, const std::string& keyword,
                      const Binary& value);

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
        getFRUsByUnexpandedLocationCode(const std::string& locationCode,
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
        getFRUsByExpandedLocationCode(const std::string& locationCode);

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
    std::string getExpandedLocationCode(const std::string& locationCode,
                                        const uint16_t nodeNumber);

    /** @brief Api to perform VPD recollection.
     * This api will trigger parser to perform VPD recollection for FRUs that
     * can be replaced at standby.
     */
    void performVPDRecollection();

  private:
    /**
     * @brief An api to process some initial requirements.
     */
    void initManager();

    /** @brief process the given JSON file
     */
    void processJSON();

    /** @brief Api to register host state callback.
     * This api will register callback to listen for host state property change.
     */
    void listenHostState();

    /** @brief Callback to listen for Host state change
     *  @param[in] msg - callback message.
     */
    void hostStateCallBack(sdbusplus::message_t& msg);

    /** @brief Api to register AssetTag property change.
     * This api will register callback to listen for asset tag property change.
     */
    void listenAssetTag();

    /** @brief Callback to listen for Asset tag change
     *  @param[in] msg - callback message.
     */
    void assetTagCallback(sdbusplus::message_t& msg);

    /**
     * @brief Restores and defaulted VPD on the system VPD EEPROM.
     *
     * This function will read the system VPD EEPROM and check if any of the
     * keywords that need to be preserved across FRU replacements are defaulted
     * in the EEPROM. If they are, this function will restore them from the
     * value that is in the D-Bus cache.
     */
    void restoreSystemVpd();

    /**
     * @brief Check for essential fru in the system.
     * The api check for the presence of FRUs marked as essential and logs PEL
     * in case they are missing.
     */
    void checkEssentialFrus();

    // Shared pointer to asio context object.
    std::shared_ptr<boost::asio::io_context>& ioContext;

    // Shared pointer to Dbus interface class.
    std::shared_ptr<sdbusplus::asio::dbus_interface>& interface;

    // Shared pointer to bus connection.
    std::shared_ptr<sdbusplus::asio::connection>& conn;

    // file to store parsed json
    nlohmann::json jsonFile;

    // map to hold mapping to inventory path to vpd file path
    // we need as map here as it is in reverse order to that of json
    inventory::FrusMap frus;

    // map to hold the mapping of location code and inventory path
    inventory::LocationCodeMap fruLocationCode;

    // map to hold FRUs which can be replaced at standby
    inventory::ReplaceableFrus replaceableFrus;

    // Shared pointer to gpio monitor object.
    std::shared_ptr<GpioMonitor> gpioMon;

    // Shared pointer to instance of the BIOS handler.
    std::shared_ptr<BiosHandler> biosHandler;

    // List of FRUs marked as essential in the system.
    inventory::EssentialFrus essentialFrus;

    // sd-bus
    sd_bus* sdBus = nullptr;
};

} // namespace manager
} // namespace vpd
} // namespace openpower
