#pragma once
#include <nlohmann/json.hpp>
#include <string_view>

namespace openpower
{
/**
 * @brief VPD parser class.
 * This class hosts functions required to parse and publish VPD data over D-Bus.
 */
class VPDParser
{
  public:
    /**
     * @brief API to parse VPD data.
     *
     * @param filePath - EEPROM path.
     * @param js - json file
     */
    void parseVPDData(const std::string& filePath, const nlohmann::json& js);
};
} // namespace openpower
