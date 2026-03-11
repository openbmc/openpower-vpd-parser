#pragma once

#include <nlohmann/json.hpp>

#include <map>
#include <string>
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
        WorkerPassKey(const WorkerPassKey&&) = delete;
        WorkerPassKey operator=(const WorkerPassKey&) = delete;
        WorkerPassKey operator=(const WorkerPassKey&&) = delete;

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
        m_systemConfigJson{i_systemConfigJson}
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
    void buildChassisToFruMap()
    {
        /* TODO:
          1. Iterate through "frus" under system config JSON
            1.i. For each FRU, iterate through the sub FRUS
                  1.i.i. For each FRU, extract Chassis ID using Object path at
          index 0, and build EEPROM to Chassis ID Map. 1.i.ii. For each sub FRU,
          use the object path to get the chassis ID, and add the sub JSON to the
          Chassis ID to Chassis JSON Map.

        */
    }

    // System config JSON
    const nlohmann::json& m_systemConfigJson;

    // Chassis ID to chassis specific JSON map - O(logN) lookup, optimized for
    // small N
    std::map<std::string, nlohmann::json> m_chassisIdToJsonMap;

    // EEPROM path to chassis ID - O(1) lookup
    std::unordered_map<std::string, std::string> m_eepromToChassisIdMap;
};

} // namespace vpd
