#pragma once
#include "error_codes.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "utility/common_utility.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <expected>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vpd
{

/**
 * @brief Class to manage configuration for all systems
 *
 * This class is meant to provide easy management of static configuration for
 * all systems. It ingests system configuration JSON and implements methods to
 * extract and expose relevant configuration for a given chassis in JSON format.
 */
class ConfigManager final
{
  public:
    /**
     * @brief Passkey class to restrict lifecycle control to Manager class only
     *
     * This is a nested class that can only be constructed by Manager.
     * It acts as a "key" that must be passed to initialize(), ensuring
     * only Manager can create or replace the singleton instance.
     */
    class ManagerPassKey
    {
      private:
        ManagerPassKey() = default;
        ~ManagerPassKey() = default;

        /* deleted methods */
        ManagerPassKey(const ManagerPassKey&) = delete;
        ManagerPassKey(ManagerPassKey&&) = delete;
        ManagerPassKey operator=(const ManagerPassKey&) = delete;
        ManagerPassKey operator=(ManagerPassKey&&) = delete;

        // Only Manager can construct this key
        friend class Manager;
    };

    // deleted methods
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

    /**
     * @brief Destructor
     */
    ~ConfigManager() = default;

    /**
     * @brief Get the singleton instance.
     *
     * Returns the shared_ptr to the ConfigManager singleton if it has
     * been initialized by Manager via initialize(), nullptr otherwise.
     *
     * @return Shared pointer to the singleton instance, or nullptr.
     */
    static std::shared_ptr<ConfigManager> getInstance() noexcept;

    /**
     * @brief Initialize the singleton with the given JSON path.
     *
     * On the first call, creates the singleton instance, parses and validates
     * the JSON at i_sysConfigJsonPath, and builds all configuration maps.
     * On subsequent calls, builds a fresh ConfigManager object entirely on
     * the side and then atomically swaps it into m_instance, so there is
     * never a window in which the singleton holds an empty or partially-built
     * JSON. Can only be called by Manager (enforced via ManagerPassKey).
     *
     * @param[in] i_key - Lifecycle key, only constructible by Manager.
     * @param[in] i_sysConfigJsonPath - Absolute path to system config JSON.
     *
     * @throw JsonException on parse or validation failure.
     *
     * @return Shared pointer to the (newly installed) singleton instance.
     */
    static std::shared_ptr<ConfigManager> initialize(
        [[maybe_unused]] const ManagerPassKey& i_key,
        const std::string& i_sysConfigJsonPath);

    /**
     * @brief API to get chassis based config JSON.
     *
     * This method returns JSON object depends on the i_vpdPath:
     * - If `std::nullopt`, the complete system configuration JSON is returned.
     * - If an EEPROM or inventory object path is provided, the corresponding
     *   chassis-specific configuration JSON is returned.
     *
     * i_vpdPath[in] i_vpdPath - Optional EEPROM or inventory object path.
     *
     * @return On success, reference to the chassis-specific JSON object.
     *         error_code::PATH_NOT_FOUND_IN_JSON if the path does not map to
     *         any known chassis.
     */
    std::expected<std::reference_wrapper<const nlohmann::json>, error_code>
        getJsonObj(const std::optional<std::string>& i_vpdPath = std::nullopt)
            const noexcept;

    /**
     * @brief API to get inventory path for a given node-qualified unexpanded
     * location code.
     *
     * With node-qualified keys, each unexpanded location code maps to exactly
     * one inventory path.
     *
     * @param[in] i_unexpandedLocationCode - Unexpanded location code
     *
     * @return Matching inventory path on success, error code otherwise.
     */
    std::expected<sdbusplus::object_path, error_code> getInventoryPath(
        const std::string& i_unexpandedLocationCode) const noexcept;

    /**
     * @brief API to get map of Chassis to its associated motherboard EEPROM
     * path
     *
     * @return Map of chassis to corresponding chassis motherboard EEPROM
     * path.
     */
    const std::map<std::string, std::string>& getChassisToMotherboardEepromMap()
        const noexcept
    {
        return m_chassisToMotherboardEepromMap;
    }

    /**
     * @brief API to get map of Chassis ID to its associated JSON configuration
     *
     * @return Map of chassis ID to corresponding chassis-specific JSON
     * configuration.
     */
    const std::map<std::string, nlohmann::json>& getChassisIdToJsonMap()
        const noexcept
    {
        return m_chassisIdToJsonMap;
    }

  private:
    /**
     * @brief Class to handle validation of configuration JSON
     *
     * This class encapsulates all JSON validation logic for system
     * configuration JSON object and chassis specific JSON objects.
     */
    class JsonValidator
    {
      public:
        /**
         * @brief Method to validate configuration JSON object
         *
         * This method performs validation of the configuration JSON
         * object to ensure it contains required sections and has proper
         * structure.
         *
         * @param[in] i_jsonObj - Configuration JSON object to validate
         *
         * @throw JsonException if validation fails
         */
        static void validateConfigJson(const nlohmann::json& i_jsonObj);

      private:
        /**
         * @brief Validate sub-FRU JSON object for mandatory and optional fields
         *
         * This method validates a sub-FRU JSON object by calling both
         * validateMandatoryTags and validateOptionalTags methods.
         *
         * @param[in] i_subFruJson - Sub-FRU JSON object to validate
         * @param[in] i_eepromPath - EEPROM path for error reporting
         * @param[in] i_index - Sub-FRU index for error reporting
         *
         * @throw JsonException if validation fails
         */
        static void validateSubFruJson(const nlohmann::json& i_subFruJson,
                                       const std::string& i_eepromPath,
                                       const size_t i_index);

        /**
         * @brief Validate mandatory tags in sub-FRU JSON object
         *
         * This method validates that all mandatory fields are present and
         * have correct types.
         *
         * @param[in] i_subFruJson - Sub-FRU JSON object to validate
         * @param[in] i_eepromPath - EEPROM path for error reporting
         * @param[in] i_index - Sub-FRU index for error reporting
         *
         * @throw JsonException if validation fails
         */
        static void validateMandatoryTags(const nlohmann::json& i_subFruJson,
                                          const std::string& i_eepromPath,
                                          const size_t i_index);

        /**
         * @brief Validate optional tags in sub-FRU JSON object
         *
         * This method validates optional fields only if they are present
         * in the JSON object.
         *
         * @param[in] i_subFruJson - Sub-FRU JSON object to validate
         * @param[in] i_eepromPath - EEPROM path for error reporting
         * @param[in] i_index - Sub-FRU index for error reporting
         *
         * @throw JsonException if validation fails
         */
        static void validateOptionalTags(const nlohmann::json& i_subFruJson,
                                         const std::string& i_eepromPath,
                                         const size_t i_index);

        /**
         * @brief Validate 'pollingRequired' tag in sub-FRU JSON object
         *
         * This method validates the 'pollingRequired' field and its nested
         * 'hotPlugging' and 'gpioPresence' objects, if present.
         *
         * @param[in] i_pollingRequiredJson - 'pollingRequired' JSON object to
         * validate
         * @param[in] i_eepromPath - EEPROM path for error reporting
         * @param[in] i_index - Sub-FRU index for error reporting
         *
         * @throw JsonException if validation fails
         */
        static void validatePollingRequiredTag(
            const nlohmann::json& i_pollingRequiredJson,
            const std::string& i_eepromPath, const size_t i_index);
    };

    /**
     * @brief API to build configuration maps
     *
     * This method iterates through the system config JSON and builds
     * the following maps:
     * 1. Chassis ID to Chassis specific JSON map : This map allows consumers to
     * get chassis configuration for a given object path.
     * 2. EEPROM path to Chassis ID map : This map allows consumers to get
     * chassis configuration for a given EEPROM path. Both these maps enable
     * O(1) chassis configuration lookup during runtime.
     * 3. Unexpanded location code to inventory path map : This map allows
     * consumers to get list of inventory paths for a given unexpanded location
     * code
     *
     *
     * @throw std::runtime_error
     */
    void buildConfigMaps();

    /**
     * @brief API to build config maps for given FRU
     *
     * This API builds following configuration maps for a given FRU :
     * - EEPROM to chassis ID map
     * - Chassis ID to JSON map
     * - Unexpanded location code to inventory path(s) map
     * - Chassis to corresponding motherboard EEPROM path map
     *
     * This API builds maps for a single FRU in the system
     * config JSON.
     *
     * @param[in] i_fruJsonObj - FRU JSON object
     * @param[in] i_commonJsonObj - JSON object which is common to all chassis
     *
     * @return On success, returns true, otherwise sets error code
     */
    std::expected<bool, error_code> buildConfigMapsForFru(
        const auto& i_fruJsonObj,
        const std::optional<nlohmann::json>& i_commonJsonObj =
            std::nullopt) noexcept;

    /**
     * @brief API to build location code to inventory path(s) map for a FRU
     *
     * This API builds the unexpanded location code to inventory path(s) map.
     * The node identifier is inserted after prefix "Ufcs/Umts".
     * of the location code to form a unique map key per (loc-code, node):
     *   "chassis"  (legacy, no suffix) -> inserts "N00"
     *   "chassis0"                     -> inserts "SC0"
     *   "chassis1"                     -> inserts "N00"
     *   "chassis2"                     -> inserts "N01"  etc.
     *
     * @param[in] i_subFruJsonArray - Sub FRU JSON array
     * @param[in] i_chassisId - Chassis ID for the FRU
     *
     * @return On success, returns true, otherwise sets error code
     */
    std::expected<bool, error_code> buildLocCodeToInvPathsMap(
        const auto& i_subFruJsonArray, const std::string& i_chassisId) noexcept;

    /**
     * @brief Extract chassis ID from given inventory object path.
     *
     * @param[in] i_inventoryObjPath - Inventory object path.
     *
     * @return Chassis ID on successful extraction, empty string otherwise.
     */
    std::string getChassisId(
        const std::string& i_inventoryObjPath) const noexcept;

    /**
     * @brief Parse a JSON file from the given path.
     *
     * Checks for file existence, non-emptiness and read access before
     * parsing. Sets o_errCode on any failure and returns an empty JSON
     * object in that case.
     *
     * @param[in] i_jsonPath - Absolute path to the JSON file.
     * @param[out] o_errCode - Error code set on failure, 0 on success.
     *
     * @return Parsed JSON object on success, empty JSON on failure.
     */
    static nlohmann::json getParsedJson(const std::string& i_jsonPath,
                                        uint16_t& o_errCode) noexcept;

    /**
     * @brief API to get unexpanded location code for given FRU JSON object
     *
     * @param[in] i_fruJsonObj - sub JSON object which represents a single FRU
     * in the system config JSON
     *
     * @return On success, returns unexpanded location code, otherwise returns
     * an error code
     */
    static std::expected<std::string, error_code>
        getUnexpandedLocationCodeForFru(
            const nlohmann::json& i_fruJsonObj) noexcept;

    /**
     * @brief API to validate chassis specific JSONs
     *
     * This API validates chassis specific JSONs which are held in the chassis
     * ID to chassis specific JSON object map data member. Each chassis specific
     * JSON is a subset of the main system configuration JSON which contains
     * details relevant to given chassis only. If any invalid chassis specific
     * JSON is detected, the corresponding entry is removed from the chassis ID
     * to JSON map.
     */
    void validateChassisSpecificJsons() noexcept;

    /**
     * @brief Load, validate and build maps from the given JSON path.
     *
     * Called by initialize() on the freshly-constructed instance.
     * Parses the JSON at i_sysConfigJsonPath, validates it, and builds all
     * configuration maps from scratch.
     *
     * @param[in] i_sysConfigJsonPath - Absolute path to system config JSON.
     *
     * @throw JsonException on parse or validation failure.
     */
    void loadJson(const std::string& i_sysConfigJsonPath);

    // Private default constructor — instances are created only via initialize()
    ConfigManager() : m_logger{Logger::getLoggerInstance()} {}

    /**
     * @brief API to check if given object path is present in the system config
     * JSON
     *
     * @param[in] i_invPath - Inventory object path
     *
     * @return true if the inventory object path is present in the system config
     * JSON, false otherwise
     */
    bool isInventoryPathInJson(const std::string& i_invPath) const noexcept;

    // Singleton instance — atomically replaced by initialize() on each call.
    static std::atomic<std::shared_ptr<ConfigManager>> m_instance;

    // System config JSON
    nlohmann::json m_systemConfigJson;

    // Chassis ID to chassis specific JSON map - O(logN) lookup, optimized for
    // small N
    std::map<std::string, nlohmann::json> m_chassisIdToJsonMap;

    // EEPROM path to chassis ID - O(1) lookup
    std::unordered_map<std::string, std::string> m_eepromToChassisIdMap;

    // Node-qualified unexpanded location code to inventory path map - O(1)
    // lookup. The key is the unexpanded location code with the node identifier
    // inserted after prefix "Ufcs/Umts". With node-qualified keys each key
    // maps to exactly one inventory path.
    std::unordered_map<std::string, sdbusplus::object_path>
        m_unexpandedLocCodeToInvPathsMap;

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger;

    // Chassis to corresponding chassis motherboard EEPROM path - O(logN)
    // lookup, optimized for small N
    std::map<std::string, std::string> m_chassisToMotherboardEepromMap;
};

} // namespace vpd
