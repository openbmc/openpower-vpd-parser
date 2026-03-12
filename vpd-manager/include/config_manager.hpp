#pragma once
#include "error_codes.hpp"
#include "logger.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
namespace vpd
{

/**
 * @brief Class to manage configuration for all systems
 *
 * This class is used for managing configuration for all systems. It
 * ingests system configuration JSON and implements methods to extract and
 * expose configuration specific JSON for a given chassis.
 */
class ConfigManager final
{
  public:
    /**
     * @brief Passkey class to restrict instantiation to Worker class only
     *
     * This is a nested class that can only be constructed by Worker.
     * It acts as a "key" that must be passed to ConfigManager's
     * constructor, ensuring only Worker can create instances.
     */
    class WorkerPassKey
    {
      private:
        WorkerPassKey() = default;
        ~WorkerPassKey() = default;

        /* deleted methods */
        WorkerPassKey(const WorkerPassKey&) = delete;
        WorkerPassKey(WorkerPassKey&&) = delete;
        WorkerPassKey operator=(const WorkerPassKey&) = delete;
        WorkerPassKey operator=(WorkerPassKey&&) = delete;

        // Only Worker can construct this key
        friend class Worker;
    };

    /**
     * @brief Constructor with passkey - can only be called by Worker
     *
     * This constructor is public but requires a WorkerPassKey that only
     * Worker can create, effectively restricting instantiation to Worker.
     *
     * @param[in] i_key - Constructor key
     * @param[in] i_systemConfigJson - System config JSON object
     *
     * @throw std::runtime_error
     */
    explicit ConfigManager([[maybe_unused]] const WorkerPassKey& i_key,
                           const nlohmann::json& i_systemConfigJson) :
        m_systemConfigJson{i_systemConfigJson},
        m_logger{Logger::getLoggerInstance()}
    {
        buildChassisToFruMap();
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

  private:
    /**
     * @brief API to build EEPROM to chassis mapping
     *
     * This method iterates through the system config JSON and builds
     * the two maps:
     * 1. Chassis ID to Chassis specific JSON map : This map allows consumers to
     * get chassis configuration for a given object path.
     * 2. EEPROM path to Chassis ID map : This map allows consumers to get
     * chassis configuration for a given EEPROM path. Both these maps enable
     * O(1) chassis configuration lookup during runtime.
     *
     * @throw std::runtime_error
     */
    void buildChassisToFruMap();

    /**
     * @brief API to extract chassis ID string from a given object path
     *
     * This API extracts the chassis ID string from a given object path.
     * For eg. object path
     * /xyz/openbmc_project/inventory/system/chassis0/motherboard/connector2 has
     * chassis ID "chassis0"
     *
     * @param[in] i_objPath - Object path
     *
     * @return On success, returns the chassis ID string, otherwise returns
     * error code
     */
    std::expected<std::string_view, error_code> getChassisIdFromObjectPath(
        const std::string_view i_objPath) const noexcept;

    /**
     * @brief API to build EEPROM to chassis map and chassis info map for a
     * single FRU
     *
     * This API builds maps for a single FRU (EEPROM path) in the system
     * config JSON.
     *
     * @param[in] i_eepromPath - EEPROM path
     * @param[in] i_fruJson - FRU JSON object
     *
     * @return On success, returns true, otherwise sets error code
     */
    std::expected<bool, error_code> buildMapForFru(
        const std::string& i_eepromPath,
        const nlohmann::json& i_fruJson) noexcept;

    // System config JSON
    const nlohmann::json& m_systemConfigJson;

    // Chassis ID to chassis specific JSON map - O(logN) lookup, optimized for
    // small N
    std::map<std::string, nlohmann::json> m_chassisIdToJsonMap;

    // EEPROM path to chassis ID - O(1) lookup
    std::unordered_map<std::string, std::string> m_eepromToChassisIdMap;

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger;
};

} // namespace vpd
