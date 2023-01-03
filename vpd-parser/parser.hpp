#pragma once
#include <nlohmann/json.hpp>
#include <store.hpp>
#include <string_view>

namespace openpower
{
namespace vpd
{

/**
 * @brief VPD parser class.
 * This class hosts functions required to parse and publish VPD data over D-Bus.
 */
class Parser
{
  public:
    /**
     * @brief API to parse VPD data.
     *
     * @param filePath - EEPROM path.
     * @param js - json file
     */
    void parseVPDData(const std::string& filePath, const nlohmann::json& js);

  private:
    /**
     * @brief An API to populate system VPD data on Dbus.
     *
     * @param vpdMap - Map of record/keyword/value.
     * @param js - json file.
     */
    void populateSystemVPDOnDbus(Parsed& vpdMap, const nlohmann::json& js);

    /**
     * @brief API to check if system VPD restore required.
     *
     * @param vpdMap - Map of Record/Keyword/Value.
     * @param objectPath - Inventory path.
     */
    void restoreSystemVPD(Parsed& vpdMap, const std::string& objectPath);
};

} // namespace vpd
} // namespace openpower
