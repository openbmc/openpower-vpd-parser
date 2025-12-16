#pragma once

#include "logger.hpp"
#include "parser_factory.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

#include <string.h>

#include <nlohmann/json.hpp>

#include <iostream>

namespace vpd
{
/**
 * @brief Class to implement a wrapper around concrete parser class.
 * The class based on VPD file passed, selects the required parser and exposes
 * API to parse the VPD and return the parsed data in required format to the
 * caller.
 */
class Parser
{
  public:
    /**
     * @brief Constructor
     *
     * @param[in] i_vpdFilePath - Path to the VPD file.
     * @param[in] i_parsedJson - Parsed JSON.
     * @param[in] i_vpdCollectionMode - VPD collection mode, default is hardware
     * mode.
     * 
     * @throw std::runtime_error
     */
    Parser(const std::string& i_vpdFilePath, nlohmann::json i_parsedJson,
           types::VpdCollectionMode i_vpdCollectionMode =
               types::VpdCollectionMode::DEFAULT_MODE);

    /**
     * @brief API to implement a generic parsing logic.
     *
     * This API is called to select parser based on the vpd data extracted from
     * the VPD file path passed to the constructor of the class.
     * It further parses the data based on the parser selected and returned
     * parsed map to the caller.
     */
    types::VPDMapVariant parse();

    /**
     * @brief API to get parser instance based on VPD type.
     *
     * This API detects the VPD type based on the file path passed to the
     * constructor of the class and returns the respective parser instance.
     *
     * @return Parser instance.
     */
    std::shared_ptr<vpd::ParserInterface> getVpdParserInstance();

    /**
     * @brief Update keyword value.
     *
     * This API is used to update keyword value on the EEPROM path and its
     * redundant path(s) if any taken from system config JSON. And also updates
     * keyword value on DBus.
     *
     * To update IPZ type VPD, input parameter for writing should be in the form
     * of (Record, Keyword, Value). Eg: ("VINI", "SN", {0x01, 0x02, 0x03}).
     *
     * To update Keyword type VPD, input parameter for writing should be in the
     * form of (Keyword, Value). Eg: ("PE", {0x01, 0x02, 0x03}).
     *
     * @param[in] i_paramsToWriteData - Input details.
     *
     * @return On success returns number of bytes written, on failure returns
     * -1.
     */
    int updateVpdKeyword(const types::WriteVpdParams& i_paramsToWriteData);

    /**
     * @brief Update keyword value.
     *
     * This API is used to update keyword value on the EEPROM path and its
     * redundant path(s) if any taken from system config JSON. And also updates
     * keyword value on DBus.
     *
     * To update IPZ type VPD, input parameter for writing should be in the form
     * of (Record, Keyword, Value). Eg: ("VINI", "SN", {0x01, 0x02, 0x03}).
     *
     * To update Keyword type VPD, input parameter for writing should be in the
     * form of (Keyword, Value). Eg: ("PE", {0x01, 0x02, 0x03}).
     *
     * @param[in] i_paramsToWriteData - Input details.
     * @param[in] o_updatedValue - Actual value which has been updated on
     * hardware.
     *
     * @return On success returns number of bytes written, on failure returns
     * -1.
     */
    int updateVpdKeyword(const types::WriteVpdParams& i_paramsToWriteData,
                         types::DbusVariantType& o_updatedValue);

    /**
     * @brief Update keyword value on hardware.
     *
     * This API is used to update keyword value on the hardware path.
     *
     * To update IPZ type VPD, input parameter for writing should be in the form
     * of (Record, Keyword, Value). Eg: ("VINI", "SN", {0x01, 0x02, 0x03}).
     *
     * To update Keyword type VPD, input parameter for writing should be in the
     * form of (Keyword, Value). Eg: ("PE", {0x01, 0x02, 0x03}).
     *
     * @param[in] i_paramsToWriteData - Input details.
     *
     * @return On success returns number of bytes written, on failure returns
     * -1.
     */
    int updateVpdKeywordOnHardware(
        const types::WriteVpdParams& i_paramsToWriteData);

  private:
    /**
     * @brief Update keyword value on redundant path.
     *
     * This API is used to update keyword value on the given redundant path(s).
     *
     * To update IPZ type VPD, input parameter for writing should be in the form
     * of (Record, Keyword, Value). Eg: ("VINI", "SN", {0x01, 0x02, 0x03}).
     *
     * To update Keyword type VPD, input parameter for writing should be in the
     * form of (Keyword, Value). Eg: ("PE", {0x01, 0x02, 0x03}).
     *
     * @param[in] i_fruPath - Redundant EEPROM path.
     * @param[in] i_paramsToWriteData - Input details.
     *
     * @return On success returns number of bytes written, on failure returns
     * -1.
     */
    int updateVpdKeywordOnRedundantPath(
        const std::string& i_fruPath,
        const types::WriteVpdParams& i_paramsToWriteData);

    // holds offfset to VPD if applicable.
    size_t m_vpdStartOffset = 0;

    // Base VPD file path used for JSON lookups.
    const std::string& m_vpdFilePath;

    // Path to configuration file, can be empty.
    nlohmann::json m_parsedJson;

    // Vector to hold VPD.
    types::BinaryVector m_vpdVector;

    // VPD collection mode, default is hardware mode.
    types::VpdCollectionMode m_vpdCollectionMode;

    /**
     * For mode-based VPD collection, m_vpdModeBasedFruPath is derived from base
     * path m_vpdFilePath and represents the actual file location.
     */
    std::string m_vpdModeBasedFruPath;

    // shared pointer to Logger object.
    std::shared_ptr<Logger> m_logger;
}; // parser
} // namespace vpd
