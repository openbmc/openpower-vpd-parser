#pragma once

#include "constants.hpp"
#include "logger.hpp"
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
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    Worker(Worker&&) = delete;
    Worker& operator=(const Worker&&) = delete;

    /**
     * @brief Constructor.
     *
     * In case the processing is not JSON based, no argument needs to be passed.
     * Constructor will also, based on symlink pick the correct JSON and
     * initialize the parsed JSON variable.
     *
     * @param[in] pathToConfigJSON - Path to the config JSON, if applicable.
     * @param[in] i_maxThreadCount - Maximum thread while collecting FRUs VPD.
     * @param[in] i_vpdCollectionMode - Mode in which VPD collection should take
     * place.
     *
     * Note: Throws std::exception in case of construction failure. Caller needs
     * to handle to detect successful object creation.
     */
    Worker(std::string pathToConfigJson = std::string(),
           uint8_t i_maxThreadCount = constants::MAX_THREADS,
           types::VpdCollectionMode i_vpdCollectionMode =
               types::VpdCollectionMode::DEFAULT_MODE);

    /**
     * @brief Destructor
     */
    ~Worker() = default;

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

    /**
     * @brief API to get list of EEPROMs for which thread creation failed.
     *
     * This API returns reference to list of EEPROM paths for which VPD
     * collection thread creation has failed. Manager needs to process this list
     * of EEPROMs and take appropriate action.
     *
     * @return reference to list of EEPROM paths for which VPD collection thread
     * creation has failed
     */
    inline std::forward_list<std::string>& getFailedEepromPaths() noexcept
    {
        return m_failedEepromPaths;
    }

    /**
     * @brief API to get VPD collection mode
     *
     * @return VPD collection mode enum value
     */
    inline types::VpdCollectionMode getVpdCollectionMode() const
    {
        return m_vpdCollectionMode;
    }

    /**
     * @brief Collect single FRU VPD
     * API can be used to perform VPD collection for the given FRU, only if the
     * current state of the system matches with the state at which the FRU is
     * allowed for VPD recollection.
     *
     * @param[in] i_dbusObjPath - D-bus object path
     */
    void collectSingleFruVpd(
        const sdbusplus::message::object_path& i_dbusObjPath);

    /**
     * @brief  Perform VPD recollection
     * This api will trigger parser to perform VPD recollection for FRUs that
     * can be replaced at standby.
     */
    void performVpdRecollection();

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
    std::tuple<bool, std::string> parseAndPublishVPD(
        const std::string& i_vpdFilePath);

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
     * @param[out] o_errCode - To set error code in case of error.
     * @return Execution status.
     */
    bool processPreAction(const std::string& i_vpdFilePath,
                          const std::string& i_flagToProcess,
                          uint16_t& o_errCode);

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
    void processAvailableProperty(const std::string& i_inventoryObjPath,
                                  types::InterfaceMap& io_interfaces);

    /**
     * @brief API to set present property.
     *
     * This API updates the present property of the given FRU with the given
     * value. Note: It is the responsibility of the caller to determine whether
     * the present property for the FRU should be updated or not.
     *
     * @param[in] i_vpdPath - EEPROM or inventory path.
     * @param[in] i_value - value to be set.
     */
    void setPresentProperty(const std::string& i_fruPath, const bool& i_value);

    /**
     * @brief API to check if the path needs to be skipped for collection.
     *
     * Some FRUs, under some given scenarios should not be collected and
     * skipped.
     *
     * @param[in] i_vpdFilePath - EEPROM path.
     *
     * @return True - if path is empty or should be skipped, false otherwise.
     */
    bool skipPathForCollection(const std::string& i_vpdFilePath);

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
        // TODO: revisit this to see if this logic can be optimized.
        return !i_fru.value("synthesized", false) &&
               i_fru.value("handlePresence", true);
    }

    /**
     * @brief API to check and execute post fail action if needed.
     *
     * This API checks if post fail action is required for a given FRU path, and
     * if needed it executes the given post fail action.
     *
     * @param[in] i_vpdFilePath - EEPROM file path.
     * @param[in] i_flowFlag - Denotes the flow w.r.t which the action should
     * be triggered.
     *
     */
    void checkAndExecutePostFailAction(
        const std::string& i_vpdFilePath,
        const std::string& i_flowFlag) const noexcept;

    // Parsed JSON file.
    nlohmann::json m_parsedJson{};

    // Path to config JSON if applicable.
    std::string& m_configJsonPath;

    // Keeps track of active thread(s) doing VPD collection.
    size_t m_activeCollectionThreadCount = 0;

    // Holds status, if VPD collection has been done or not.
    // Note: This variable does not give information about successfull or failed
    // collection. It just states, if the VPD collection process is over or not.
    bool m_isAllFruCollected = false;

    // Mutex to guard critical resource m_activeCollectionThreadCount.
    std::mutex m_mutex;

    // Counting semaphore to limit the number of threads.
    std::counting_semaphore<constants::MAX_THREADS> m_semaphore;

    // List of EEPROM paths for which VPD collection thread creation has failed.
    std::forward_list<std::string> m_failedEepromPaths;

    // VPD collection mode
    types::VpdCollectionMode m_vpdCollectionMode{
        types::VpdCollectionMode::DEFAULT_MODE};

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger;
};
} // namespace vpd
