#pragma once

#include "constants.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>

#include <mutex>
#include <optional>
#include <semaphore>
#include <tuple>

namespace vpd
{
/**
 * @brief A class to process and publish VPD data.
 *
 * The class works on VPD and is mainly responsible for following tasks:
 * 1) Select appropriate device tree and JSON. Reboot if required.
 * 2) Get desired parser using parser factory.
 * 3) Calling respective parser class to get parsed VPD.
 * 4) Arranging VPD data under required interfaces.
 * 5) Calling PIM to publish VPD.
 *
 * The class may also implement helper functions required for VPD handling.
 */
class Worker
{
  public:
    /**
     * List of deleted functions.
     */
    Worker(const Worker&);
    Worker& operator=(const Worker&);
    Worker(Worker&&) = delete;

    /**
     * @brief Constructor.
     *
     * In case the processing is not JSON based, no argument needs to be passed.
     * Constructor will also, based on symlink pick the correct JSON and
     * initialize the parsed JSON variable.
     *
     * @param[in] pathToConfigJSON - Path to the config JSON, if applicable.
     *
     * Note: Throws std::exception in case of construction failure. Caller needs
     * to handle to detect successful object creation.
     */
    Worker(std::string pathToConfigJson = std::string());

    /**
     * @brief Destructor
     */
    ~Worker() = default;

#ifdef IBM_SYSTEM
    /**
     * @brief API to perform initial setup before manager claims Bus name.
     *
     * Before BUS name for VPD-Manager is claimed, fitconfig whould be set for
     * corret device tree, inventory JSON w.r.t system should be linked and
     * system VPD should be on DBus.
     */
    void performInitialSetup();
#endif

    /**
     * @brief An API to check if system VPD is already published.
     *
     * @return Status, true if system is already collected else false.
     */
    bool isSystemVPDOnDBus() const;

    /**
     * @brief API to process all FRUs presnt in config JSON file.
     *
     * This API based on config JSON passed/selected for the system, will
     * trigger parser for all the FRUs and publish it on DBus.
     *
     * Note: Config JSON file path should be passed to worker class constructor
     * to make use of this API.
     *
     */
    void collectFrusFromJson();

    /**
     * @brief API to parse VPD data
     *
     * @param[in] i_vpdFilePath - Path to the VPD file.
     */
    types::VPDMapVariant parseVpdFile(const std::string& i_vpdFilePath);

    /**
     * @brief An API to populate DBus interfaces for a FRU.
     *
     * Note: Call this API to populate D-Bus. Also caller should handle empty
     * objectInterfaceMap.
     *
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     * @param[out] objectInterfaceMap - Object and its interfaces map.
     * @param[in] vpdFilePath - EEPROM path of FRU.
     */
    void populateDbus(const types::VPDMapVariant& parsedVpdMap,
                      types::ObjectMap& objectInterfaceMap,
                      const std::string& vpdFilePath);

    /**
     * @brief An API to delete FRU VPD over DBus.
     *
     * @param[in] i_dbusObjPath - Dbus object path of the FRU.
     *
     * @throw std::runtime_error if given input path is empty.
     */
    void deleteFruVpd(const std::string& i_dbusObjPath);

    /**
     * @brief API to get status of VPD collection process.
     *
     * @return - True when done, false otherwise.
     */
    inline bool isAllFruCollectionDone() const
    {
        return m_isAllFruCollected;
    }

    /**
     * @brief API to get system config JSON object
     *
     * @return System config JSON object.
     */
    inline nlohmann::json getSysCfgJsonObj() const
    {
        return m_parsedJson;
    }

    /**
     * @brief API to get active thread count.
     *
     * Each FRU is collected in a separate thread. This API gives the active
     * thread collecting FRU's VPD at any given time.
     *
     * @return Count of active threads.
     */
    size_t getActiveThreadCount() const
    {
        return m_activeCollectionThreadCount;
    }

  private:
    /**
     * @brief An API to parse and publish a FRU VPD over D-Bus.
     *
     * Note: This API will handle all the exceptions internally and will only
     * return status of parsing and publishing of VPD over D-Bus.
     *
     * @param[in] i_vpdFilePath - Path of file containing VPD.
     * @return Tuple of status and file path. Status, true if successfull else
     * false.
     */
    std::tuple<bool, std::string>
        parseAndPublishVPD(const std::string& i_vpdFilePath);

    /**
     * @brief An API to set appropriate device tree and JSON.
     *
     * This API based on system chooses corresponding device tree and JSON.
     * If device tree change is required, it updates the "fitconfig" and reboots
     * the system. Else it is NOOP.
     *
     * @throw std::runtime_error
     */
    void setDeviceTreeAndJson();

    /**
     * @brief API to select system specific JSON.
     *
     * The API based on the IM value of VPD, will select appropriate JSON for
     * the system. In case no system is found corresponding to the extracted IM
     * value, error will be logged.
     *
     * @param[out] systemJson - System JSON name.
     * @param[in] parsedVpdMap - Parsed VPD map.
     */
    void getSystemJson(std::string& systemJson,
                       const types::VPDMapVariant& parsedVpdMap);

    /**
     * @brief An API to read IM value from VPD.
     *
     * Note: Throws exception in case of error. Caller need to handle.
     *
     * @param[in] parsedVpd - Parsed VPD.
     */
    std::string getIMValue(const types::IPZVpdMap& parsedVpd) const;

    /**
     * @brief An API to read HW version from VPD.
     *
     * Note: Throws exception in case of error. Caller need to handle.
     *
     * @param[in] parsedVpd - Parsed VPD.
     */
    std::string getHWVersion(const types::IPZVpdMap& parsedVpd) const;

    /**
     * @brief An API to parse given VPD file path.
     *
     * @param[in] vpdFilePath - EEPROM file path.
     * @param[out] parsedVpd - Parsed VPD as a map.
     */
    void fillVPDMap(const std::string& vpdFilePath,
                    types::VPDMapVariant& parsedVpd);

    /**
     * @brief An API to parse and publish system VPD on D-Bus.
     *
     * Note: Throws exception in case of invalid VPD format.
     *
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     */
    void publishSystemVPD(const types::VPDMapVariant& parsedVpdMap);

    /**
     * @brief An API to process extrainterfaces w.r.t a FRU.
     *
     * @param[in] singleFru - JSON block for a single FRU.
     * @param[out] interfaces - Map to hold interface along with its properties.
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     */
    void processExtraInterfaces(const nlohmann::json& singleFru,
                                types::InterfaceMap& interfaces,
                                const types::VPDMapVariant& parsedVpdMap);

    /**
     * @brief An API to process embedded and synthesized FRUs.
     *
     * @param[in] singleFru - FRU to be processed.
     * @param[out] interfaces - Map to hold interface along with its properties.
     */
    void processEmbeddedAndSynthesizedFrus(const nlohmann::json& singleFru,
                                           types::InterfaceMap& interfaces);

    /**
     * @brief An API to read process FRU based in CCIN.
     *
     * For some FRUs VPD can be processed only if the FRU has some specific
     * value for CCIN. In case the value is not from that set, VPD for those
     * FRUs can't be processed.
     *
     * @param[in] singleFru - Fru whose CCIN value needs to be matched.
     * @param[in] parsedVpdMap - Parsed VPD map.
     */
    bool processFruWithCCIN(const nlohmann::json& singleFru,
                            const types::VPDMapVariant& parsedVpdMap);

    /**
     * @brief API to process json's inherit flag.
     *
     * Inherit flag denotes that some property in the child FRU needs to be
     * inherited from parent FRU.
     *
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     * @param[out] interfaces - Map to hold interface along with its properties.
     */
    void processInheritFlag(const types::VPDMapVariant& parsedVpdMap,
                            types::InterfaceMap& interfaces);

    /**
     * @brief API to process json's "copyRecord" flag.
     *
     * copyRecord flag denotes if some record data needs to be copies in the
     * given FRU.
     *
     * @param[in] singleFru - FRU being processed.
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     * @param[out] interfaces - Map to hold interface along with its properties.
     */
    void processCopyRecordFlag(const nlohmann::json& singleFru,
                               const types::VPDMapVariant& parsedVpdMap,
                               types::InterfaceMap& interfaces);

    /**
     * @brief An API to populate IPZ VPD property map.
     *
     * @param[out] interfacePropMap - Map of interface and properties under it.
     * @param[in] keyordValueMap - Keyword value map of IPZ VPD.
     * @param[in] interfaceName - Name of the interface.
     */
    void populateIPZVPDpropertyMap(types::InterfaceMap& interfacePropMap,
                                   const types::IPZKwdValueMap& keyordValueMap,
                                   const std::string& interfaceName);

    /**
     * @brief An API to populate Kwd VPD property map.
     *
     * @param[in] keyordValueMap - Keyword value map of Kwd VPD.
     * @param[out] interfaceMap - interface and property,value under it.
     */
    void populateKwdVPDpropertyMap(const types::KeywordVpdMap& keyordVPDMap,
                                   types::InterfaceMap& interfaceMap);

    /**
     * @brief API to populate all required interface for a FRU.
     *
     * @param[in] interfaceJson - JSON containing interfaces to be populated.
     * @param[out] interfaceMap - Map to hold populated interfaces.
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     */
    void populateInterfaces(const nlohmann::json& interfaceJson,
                            types::InterfaceMap& interfaceMap,
                            const types::VPDMapVariant& parsedVpdMap);

    /**
     * @brief Helper function to insert or merge in map.
     *
     * This method checks in the given inventory::InterfaceMap if the given
     * interface key is existing or not. If the interface key already exists,
     * given property map is inserted into it. If the key does'nt exist then
     * given interface and property map pair is newly created. If the property
     * present in propertymap already exist in the InterfaceMap, then the new
     * property value is ignored.
     *
     * @param[in,out] interfaceMap - map object of type inventory::InterfaceMap
     * only.
     * @param[in] interface - Interface name.
     * @param[in] property - new property map that needs to be emplaced.
     */
    void insertOrMerge(types::InterfaceMap& interfaceMap,
                       const std::string& interface,
                       types::PropertyMap&& property);

    /**
     * @brief Check if the given CPU is an IO only chip.
     *
     * The CPU is termed as IO, whose all of the cores are bad and can never be
     * used. Those CPU chips can be used for IO purpose like connecting PCIe
     * devices etc., The CPU whose every cores are bad, can be identified from
     * the CP00 record's PG keyword, only if all of the 8 EQs' value equals
     * 0xE7F9FF. (1EQ has 4 cores grouped together by sharing its cache memory.)
     *
     * @param [in] pgKeyword - PG Keyword of CPU.
     * @return true if the given cpu is an IO, false otherwise.
     */
    bool isCPUIOGoodOnly(const std::string& pgKeyword);

    /**
     * @brief API to prime inventory Objects.
     *
     * @param[in] i_vpdFilePath - EEPROM file path.
     * @return true if the prime inventory is success, false otherwise.
     */
    bool primeInventory(const std::string& i_vpdFilePath);

    /**
     * @brief API to process preAction(base_action) defined in config JSON.
     *
     * @note sequence of tags under any given flag of preAction is EXTREMELY
     * important to ensure proper processing. The API will process all the
     * nested items under the base action sequentially. Also if any of the tag
     * processing fails, the code will not process remaining tags under the
     * flag.
     * ******** sample format **************
     * fru EEPROM path: {
     *     base_action: {
     *         flag1: {
     *           tag1: {
     *            },
     *           tag2: {
     *            }
     *         }
     *         flag2: {
     *           tags: {
     *            }
     *         }
     *     }
     * }
     * *************************************
     *
     * @param[in] i_vpdFilePath - Path to the EEPROM file.
     * @param[in] i_flagToProcess - To identify which flag(s) needs to be
     * processed under PreAction tag of config JSON.
     * @return Execution status.
     */
    bool processPreAction(const std::string& i_vpdFilePath,
                          const std::string& i_flagToProcess);

    /**
     * @brief API to process postAction(base_action) defined in config JSON.
     *
     * @note Sequence of tags under any given flag of postAction is EXTREMELY
     * important to ensure proper processing. The API will process all the
     * nested items under the base action sequentially. Also if any of the tag
     * processing fails, the code will not process remaining tags under the
     * flag.
     * ******** sample format **************
     * fru EEPROM path: {
     *     base_action: {
     *         flag1: {
     *           tag1: {
     *            },
     *           tag2: {
     *            }
     *         }
     *         flag2: {
     *           tags: {
     *            }
     *         }
     *     }
     * }
     * *************************************
     * Also, if post action is required to be processed only for FRUs with
     * certain CCIN then CCIN list can be provided under flag.
     *
     * @param[in] i_vpdFruPath - Path to the EEPROM file.
     * @param[in] i_flagToProcess - To identify which flag(s) needs to be
     * processed under postAction tag of config JSON.
     * @param[in] i_parsedVpd - Optional Parsed VPD map. If CCIN match is
     * required.
     * @return Execution status.
     */
    bool processPostAction(
        const std::string& i_vpdFruPath, const std::string& i_flagToProcess,
        const std::optional<types::VPDMapVariant> i_parsedVpd = std::nullopt);

    /**
     * @brief Function to enable and bring MUX out of idle state.
     *
     * This finds all the MUX defined in the system json and enables them by
     * setting the holdidle parameter to 0.
     *
     * @throw std::runtime_error
     */
    void enableMuxChips();

    /**
     * @brief An API to perform backup or restore of VPD.
     *
     * @param[in,out] io_srcVpdMap - Source VPD map.
     */
    void performBackupAndRestore(types::VPDMapVariant& io_srcVpdMap);

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
     * @param[in] io_interfaces - Map to hold all the interfaces for the FRU.
     */
    void processFunctionalProperty(const std::string& i_inventoryObjPath,
                                   types::InterfaceMap& io_interfaces);

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
     * @param[in] io_interfaces - Map to hold all the interfaces for the FRU.
     */
    void processEnabledProperty(const std::string& i_inventoryObjPath,
                                types::InterfaceMap& io_interfaces);

    /**
     * @brief API to form asset tag string for the system.
     *
     * @param[in] i_parsedVpdMap - Parsed VPD map.
     *
     * @throw std::runtime_error
     *
     * @return - Formed asset tag string.
     */
    std::string
        createAssetTagString(const types::VPDMapVariant& i_parsedVpdMap);

    /**
     * @brief API to prime system blueprint.
     *
     * The API will traverse the system config JSON and will prime all the FRU
     * paths which qualifies for priming.
     */
    void primeSystemBlueprint();

    /**
     * @brief API to set symbolic link for system config JSON.
     *
     * Once correct device tree is set, symbolic link to the correct sytsem
     * config JSON is set to be used in subsequent BMC boot.
     *
     * @param[in] i_systemJson - system config JSON.
     */
    void setJsonSymbolicLink(const std::string& i_systemJson);

    // Parsed JSON file.
    nlohmann::json m_parsedJson{};

    // Hold if symlink is present or not.
    bool m_isSymlinkPresent = false;

    // Path to config JSON if applicable.
    std::string& m_configJsonPath;

    // Keeps track of active thread(s) doing VPD collection.
    size_t m_activeCollectionThreadCount = 0;

    // Holds status, if VPD collection has been done or not.
    // Note: This variable does not give information about successfull or failed
    // collection. It just states, if the VPD collection process is over or not.
    bool m_isAllFruCollected = false;

    // To distinguish the factory reset path.
    bool m_isFactoryResetDone = false;

    // Mutex to guard critical resource m_activeCollectionThreadCount.
    std::mutex m_mutex;

    // Counting semaphore to limit the number of threads.
    std::counting_semaphore<constants::MAX_THREADS> m_semaphore{
        constants::MAX_THREADS};
};
} // namespace vpd
