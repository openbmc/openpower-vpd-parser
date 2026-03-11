#pragma once
#include "worker.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace vpd
{

/**
 * @brief Class to manage configuration for multi-chassis systems
 *
 * This class is used for managing configuration for multi-chassis systems. It
 * ingests system configuration JSON and implements methods to extract and
 * expose configuration specific to a given chassis.
 */
class ConfigManager final
{
  public:
    /**
     * @brief Passkey class to restrict instantiation to Worker class only
     *
     * This is a private nested class that can only be constructed by Worker.
     * It acts as a "key" that must be passed to ConfigManager's
     * constructor, ensuring only Worker can create instances.
     */
    class ConstructorKey
    {
      private:
        // Only Worker can construct this key
        ConstructorKey() = default;
        friend class Worker;
    };

    /**
     * @brief Constructor with passkey - can only be called by Worker
     *
     * This constructor is public but requires a ConstructorKey that only
     * Worker can create, effectively restricting instantiation to Worker.
     *
     * @param[in] i_key - Constructor key (only Worker can create this)
     * @param[in] i_systemConfigJson - System config JSON object
     *
     * @throw std::runtime_error
     */
    explicit ConfigManager([[maybe_unused]] ConstructorKey i_key,
                           const nlohmann::json& i_systemConfigJson) :
        m_systemConfigJson{i_systemConfigJson}
    {
        buildChassisToFruMap();
    }

    /**
     * @brief Deleted copy constructor
     */
    ConfigManager(const ConfigManager&) = delete;

    /**
     * @brief Deleted copy assignment operator
     */
    ConfigManager& operator=(const ConfigManager&) = delete;

    /**
     * @brief Deleted move constructor
     */
    ConfigManager(ConfigManager&&) = delete;

    /**
     * @brief Deleted move assignment operator
     */
    ConfigManager& operator=(ConfigManager&&) = delete;

    /**
     * @brief Destructor
     */
    ~ConfigManager() = default;

  private:
    // Chassis information structure
    struct ChassisInfo
    {
        std::string m_chassisId; // string representing chassis, for eg.
                                 // chassis0, chassis1, etc.
        std::string
            m_chassisPath;       // object path for the chassis, for eg.
                           // /xyz/openbmc_project/inventory/system/chassis0
        nlohmann::json m_chassisConfig; // Chassis-specific config
    };

    /**
     * @brief API to build EEPROM to chassis mapping
     *
     * This method iterates through the system config JSON and builds
     * the two maps:
     * 1. Chassis ID to Chassis info map : This map allows consumers to get
     * chassis configuration for a given object path.
     * 2. EEPROM path to Chassis ID map : This map allows consumers to get
     * chassis configuration for a given EEPROM path. Both these maps enable
     * O(1) chassis configuration lookup during runtime.
     */
    void buildChassisToFruMap()
    {
        /* TODO:
          1. Iterate through "frus" under system config JSON
            1.i. For each FRU, iterate through the sub FRUS
                  1.i.i. For each FRU, extract Chassis ID using Object path at
          index 0, and build EEPROM to Chassis ID Map. 1.i.ii. For each sub FRU,
          use the object path to get the chassis ID, and add the sub JSON to the
          Chassis ID to Chassis Info Map.

        */
    }

    // System config JSON
    nlohmann::json m_systemConfigJson;

    // Chassis ID to chassis info map - O(1) lookup
    std::unordered_map<std::string, ChassisInfo> m_chassisInfoMap;

    // EEPROM path to chassis ID - O(1) lookup
    std::unordered_map<std::string, std::string> m_eepromToChassisIdMap;
};

} // namespace vpd
