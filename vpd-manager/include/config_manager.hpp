#pragma once
#include "error_codes.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "utility/common_utility.hpp"
#include "utility/json_utility.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <format>
#include <map>
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
     * @brief Passkey class to restrict instantiation to Manager class only
     *
     * This is a nested class that can only be constructed by Manager.
     * It acts as a "key" that must be passed to ConfigManager's
     * constructor, ensuring only Manager can create instances.
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

    /**
     * @brief Constructor with passkey - can only be called by Manager
     *
     * This constructor is public but requires a ManagerPassKey that only
     * Manager can create, effectively restricting instantiation to Manager.
     *
     * @param[in] i_key - Constructor key
     * @param[in] i_sysConfigJsonPath - Absolute path to system config JSON
     *
     * @throw std::runtime_error
     */
    explicit ConfigManager([[maybe_unused]] const ManagerPassKey& i_key,
                           const std::string& i_sysConfigJsonPath) :
        m_logger{Logger::getLoggerInstance()}
    {
        uint16_t l_errCode{constants::VALUE_0};

        m_systemConfigJson =
            jsonUtility::getParsedJson(i_sysConfigJsonPath, l_errCode);

        if (l_errCode != constants::VALUE_0)
        {
            throw JsonException{std::format(
                "ConfigManager initialization failed. Reason: Failed to parse JSON from path {}. Error : {}",
                i_sysConfigJsonPath, commonUtility::getErrCodeMsg(l_errCode))};
        }

        buildConfigMaps();
    }

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
     * @brief API to get chassis based config JSON.
     *
     * This method returns JSON object depends on the i_vpdPath:
     * - If `std::nullopt`, the complete system configuration JSON is returned.
     * - If an EEPROM or inventory object path is provided, the corresponding
     *   chassis-specific configuration JSON is returned.
     *
     * i_vpdPath[in] i_vpdPath - Optional EEPROM or inventory object path.
     *
     * @return Reference to the selected configuration JSON object.
     */
    const nlohmann::json& getJsonObj(
        const std::optional<std::string>& i_vpdPath =
            std::nullopt) const noexcept;

    /**
     * @brief API to get inventory path(s) for given unexpanded location code
     *
     * This API returns inventory path(s) for given unexpanded location code. An
     * unexpanded location code can map to single or multiple inventory paths,
     * this API returns a single or a list of matching inventory paths.
     *
     * @param[in] i_unexpandedLocationCode - Unexpanded location code
     *
     * @return Vector containing matching inventory paths, error code otherwise.
     */
    std::expected<types::ListOfPaths, error_code> getInventoryPaths(
        const std::string& i_unexpandedLocationCode) const noexcept;

  private:
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
     * This API builds following configuration maps for a given FRU :
     * - Unexpanded location code to inventory path(s) map
     *
     * @param[in] i_subFruJsonArray - Sub FRU JSON array
     *
     * @return On success, returns true, otherwise sets error code
     */
    std::expected<bool, error_code> buildLocCodeToInvPathsMap(
        const auto& i_subFruJsonArray) noexcept;

    /**
     * @brief Extract chassis ID from given inventory object path.
     *
     * @param[in] i_inventoryObjPath - Inventory object path.
     *
     * @return Chassis ID on successful extraction, empty string otherwise.
     */
    std::string getChassisId(
        const std::string& i_inventoryObjPath) const noexcept;

    // System config JSON
    nlohmann::json m_systemConfigJson;

    // Chassis ID to chassis specific JSON map - O(logN) lookup, optimized for
    // small N
    std::map<std::string, nlohmann::json> m_chassisIdToJsonMap;

    // EEPROM path to chassis ID - O(1) lookup
    std::unordered_map<std::string, std::string> m_eepromToChassisIdMap;

    // Unexpanded location code to inventory paths map - O(1) lookup
    // since same location code can be mapped to multiple
    // inventory paths, use a vector of paths as the value
    std::unordered_map<std::string, types::ListOfPaths>
        m_unexpandedLocCodeToInvPathsMap;

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger;
};

} // namespace vpd
