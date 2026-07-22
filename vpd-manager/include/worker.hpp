#pragma once

#include "config_manager.hpp"
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
 * 1) Get desired parser using parser factory.
 * 2) Calling respective parser class to get parsed VPD.
 * 3) Arranging VPD data under required interfaces.
 * 4) Calling PIM to publish VPD.
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
     * @throw std::bad_alloc
     */
    Worker() : m_logger(Logger::getLoggerInstance()) {};

    /**
     * @brief Destructor
     */
    ~Worker() = default;

    /**
     * @brief API to parse VPD data
     *
     * @param[in] i_configJsonObj - Config json object.
     * @param[in] i_vpdFilePath - Path to the VPD file.
     * @param[in] i_processRedundant - Enables VPD collection for redundant
     * EEPROM path.
     * @param[out] o_presenceState - Set to true if the FRU is present, false
     * otherwise. Determined from pre-action result or file existence.
     *
     * @note When processing redundant paths and `i_processRedundant` is false,
     * only the FRU pre-action is performed and VPD collection, post-action
     * actions are skipped.
     */
    types::VPDMapVariant parseVpdFile(
        const nlohmann::json& i_configJsonObj, const std::string& i_vpdFilePath,
        const bool& i_processRedundant, bool& o_presenceState);

    /**
     * @brief An API to populate DBus interfaces for a FRU.
     *
     * Note: Call this API to populate D-Bus. Also caller should handle empty
     * objectInterfaceMap.
     *
     * @param[in] i_configJsonObj - Config json object.
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     * @param[out] objectInterfaceMap - Object and its interfaces map.
     * @param[in] vpdFilePath - EEPROM path of FRU.
     */
    void populateDbus(const nlohmann::json& i_configJsonObj,
                      const types::VPDMapVariant& parsedVpdMap,
                      types::ObjectMap& objectInterfaceMap,
                      const std::string& vpdFilePath);

    /**
     * @brief An API to delete FRU VPD over DBus.
     *
     * @param[in] i_configJsonObj - Config json.
     *
     * @param[in] i_dbusObjPath - Dbus object path of the FRU.
     *
     * @throw std::runtime_error if given input path is empty.
     */
    void deleteFruVpd(const nlohmann::json& i_configJsonObj,
                      const std::string& i_dbusObjPath);

    /**
     * @brief Collect single FRU VPD
     * API can be used to perform VPD collection for the given FRU, only if the
     * current state of the system matches with the state at which the FRU is
     * allowed for VPD recollection.
     *
     * @param[in] i_configJsonObj - Config Json object.
     * @param[in] i_dbusObjPath - D-bus object path
     */
    void collectSingleFruVpd(const nlohmann::json& i_configJsonObj,
                             const sdbusplus::object_path& i_dbusObjPath);

    /**
     * @brief  Perform VPD recollection
     *
     * This api will trigger parser to perform VPD recollection for FRUs that
     * can be replaced at standby.
     *
     * @param[in] i_sysCfgJsonObj - System config JSON object.
     */
    void performVpdRecollection(const nlohmann::json& i_sysCfgJsonObj) noexcept;

    /**
     * @brief API to process FRU collection
     *
     * This API processes a single FRU's VPD collection based on the provided
     * JSON configuration.
     *
     * @param[in] i_fruPath - Path to the FRU EEPROM file
     * @param[in] i_cfgJsonObj - Config JSON object
     * @param[out] o_errCode - To set error code in case of exception
     *
     * @return Tuple of FRU presence and VPD collection status of the FRU
     */
    std::tuple<bool, std::string> collectFruVpd(
        const std::string& i_fruPath, const nlohmann::json& i_cfgJsonObj,
        uint16_t& o_errCode) noexcept;

  private:
    /**
     * @brief Execute pre-action for a redundant VPD path without parsing VPD.
     *
     * Invokes parseVpdFile for the given EEPROM path in a mode where only
     * pre-action is executed. Actual VPD parsing and post actions are
     * skipped.
     *
     * @param[in] i_configJson - Config Json object.
     * @param[in] i_eepromFilePath - Redundant EEPROM file path.
     *
     * @return true if pre-action execution succeeds, false otherwise.
     */
    bool processRedundantPreAction(
        const nlohmann::json& i_configJson,
        const std::string& i_eepromFilePath) noexcept;

    /**
     * @brief An API to parse and publish a FRU VPD over D-Bus.
     *
     * Note: This API will handle all the exceptions internally and will return
     * the tuple of collection status and the presence state of the FRU.
     *
     * @param[in] i_configJson - Config Json object.
     * @param[in] i_vpdFilePath - Path of file containing VPD.
     * @param[in] i_processRedundant - Enables VPD collection for redundant
     * EEPROM path.
     *
     * @note When processing redundant paths and `i_processRedundant` is false,
     * only the FRU pre-action is performed and VPD collection, post-action
     * actions are skipped.
     *
     * @return Tuple of <collection success, FRU presence>.
     * First bool: true if VPD collection succeeded, false otherwise.
     * Second bool: true if FRU is present, false otherwise.
     */
    std::tuple<bool, bool> parseAndPublishVPD(
        const nlohmann::json& i_configJson, const std::string& i_vpdFilePath,
        const bool& i_processRedundant = false);

    /**
     * @brief An API to process extrainterfaces w.r.t a FRU.
     *
     * @param[in] singleFru - JSON block for a single FRU.
     * @param[out] interfaces - Map to hold interface along with its properties.
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     * @param[in] i_inventoryPath - Inventory object path.
     */
    void processExtraInterfaces(const nlohmann::json& singleFru,
                                types::InterfaceMap& interfaces,
                                const types::VPDMapVariant& parsedVpdMap,
                                const std::string& i_inventoryPath);

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
     * @param[in] i_configJson - Config Json object.
     * @param[in] parsedVpdMap - Parsed VPD as a map.
     * @param[out] interfaces - Map to hold interface along with its
     * properties.
     * @param[in] i_inventoryPath - inventory path.
     */
    void processInheritFlag(const nlohmann::json& i_configJson,
                            const types::VPDMapVariant& parsedVpdMap,
                            types::InterfaceMap& interfaces,
                            const std::string& i_inventoryPath);

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
     * @param[in] i_inventoryPath - inventory Path.
     */
    void populateInterfaces(const nlohmann::json& interfaceJson,
                            types::InterfaceMap& interfaceMap,
                            const types::VPDMapVariant& parsedVpdMap,
                            const std::string& i_inventoryPath);

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
     * @param[in] i_configJson - Config Json object.
     * @param[in] i_vpdFilePath - Path to the EEPROM file.
     * @param[in] i_flagToProcess - To identify which flag(s) needs to be
     * processed under PreAction tag of config JSON.
     * @param[out] o_errCode - To set error code in case of error.
     * @return types::BaseActionResult with execution status and presence info.
     */
    types::BaseActionResult processPreAction(
        const nlohmann::json& i_configJson, const std::string& i_vpdFilePath,
        const std::string& i_flagToProcess, uint16_t& o_errCode);

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
     * @param[in] i_configJson - Config Json object.
     * @param[in] i_vpdFruPath - Path to the EEPROM file.
     * @param[in] i_flagToProcess - To identify which flag(s) needs to be
     * processed under postAction tag of config JSON.
     * @param[in] i_parsedVpd - Optional Parsed VPD map. If CCIN match is
     * required.
     * @return Execution status.
     */
    bool processPostAction(
        const nlohmann::json& i_configJson, const std::string& i_vpdFruPath,
        const std::string& i_flagToProcess,
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
     * @brief API to set present property.
     *
     * This API updates the present property of the given FRU with the given
     * value. Note: It is the responsibility of the caller to determine whether
     * the present property for the FRU should be updated or not.
     *
     * @param[in] i_configJson - Config Json object.
     * @param[in] i_vpdPath - EEPROM or inventory path.
     * @param[in] i_value - value to be set.
     */
    void setPresentProperty(const nlohmann::json& i_configJson,
                            const std::string& i_fruPath, const bool& i_value);

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

    /**
     * @brief API to process json's "skipRecords" flag.
     *
     * This API iterates through the parsed VPD map and copies all records of
     * given FRU to the target except those explicitly defined in the
     * 'skipRecords' array for that FRU, within the JSON configuration.
     *
     * @param[in] i_fruJson - FRU json object
     * @param[in] i_parsedVpdMap - Parsed VPD as a map.
     * @param[out] o_interfaces - The map to be populated with non-skipped
     * records.
     *
     */
    void processSkipRecordsFlag(const nlohmann::json& i_fruJson,
                                const types::VPDMapVariant& i_parsedVpdMap,
                                types::InterfaceMap& o_interfaces);

    // List of EEPROM paths for which VPD collection thread creation has failed.
    std::forward_list<std::string> m_failedEepromPaths;

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger;
};
} // namespace vpd
