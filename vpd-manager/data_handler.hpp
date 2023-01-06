#pragma once
#include <iostream>
#include <nlohmann/json.hpp>
#include <types.hpp>

namespace openpower
{
namespace vpd
{

/**
 * @brief A class to handle VPD data.
 * This class hosts functions required to parse and publish VPD data over D-Bus.
 */
class DataHandler
{
  public:
    /**
     * @brief API to parse VPD data.
     *
     * @param filePath - EEPROM path.
     * @param js - json file
     * @param parsedData - Map of parsed VPD data.
     */
    void parseVPDData(const std::string& filePath, const nlohmann::json& js,
                      types::parsedVPDMap& parsedData);
};

} // namespace vpd
} // namespace openpower
