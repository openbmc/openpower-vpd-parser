#pragma once

#include "tool_utils.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace vpd
{
/**
 * @brief Class to support operations on VPD.
 *
 * The class provides API's to,
 * Read keyword value from DBus/hardware.
 * Update keyword value to DBus/hardware.
 * Dump DBus object's critical information.
 * Fix system VPD if any mismatch between DBus and hardware data.
 * Reset specific system VPD keywords to its default value.
 * Force VPD collection for hardware.
 */
class VpdTool
{
  private:
    /**
     * @brief Get specific properties of a FRU in JSON format.
     *
     * For a given object path of a FRU, this API returns the following
     * properties of the FRU in JSON format:
     * - Pretty Name, Location Code, Sub Model
     * - SN, PN, CC, FN, DR keywords under VINI record.
     *
     * @param[in] i_objectPath - DBus object path
     *
     * @return On success, returns the properties of the FRU in JSON format,
     * otherwise returns an empty JSON.
     * If FRU's "Present" property is false, this API returns an empty JSON.
     * Note: The caller of this API should handle empty JSON.
     *
     * @throw json::exception
     */
    nlohmann::json getFruProperties(const std::string& i_objectPath) const;

    /**
     * @brief Get any inventory property in JSON.
     *
     * API to get any property of a FRU in JSON format. Given an object path,
     * interface and property name, this API does a D-Bus read property on PIM
     * to get the value of that property and returns it in JSON format. This API
     * returns empty JSON in case of failure. The caller of the API must check
     * for empty JSON.
     *
     * @param[in] i_objectPath - DBus object path
     * @param[in] i_interface - Interface name
     * @param[in] i_propertyName - Property name
     *
     * @return On success, returns the property and its value in JSON format,
     * otherwise return empty JSON.
     * {"SN" : "ABCD"}
     */
    template <typename PropertyType>
    nlohmann::json getInventoryPropertyJson(
        const std::string& i_objectPath, const std::string& i_interface,
        const std::string& i_propertyName) const noexcept;

    /**
     * @brief Get the "type" property for a FRU.
     *
     * Given a FRU path, and parsed System Config JSON, this API returns the
     * "type" property for the FRU in JSON format. This API gets
     * these properties from Phosphor Inventory Manager.
     *
     * @param[in] i_objectPath - DBus object path.
     *
     * @return On success, returns the "type" property in JSON
     * format, otherwise returns empty JSON. The caller of this API should
     * handle empty JSON.
     */
    nlohmann::json
        getFruTypeProperty(const std::string& i_objectPath) const noexcept;

    /**
     * @brief Check if a FRU is present in the system.
     *
     * Given a FRU's object path, this API checks if the FRU is present in the
     * system by reading the "Present" property of the FRU.
     *
     * @param[in] i_objectPath - DBus object path.
     *
     * @return true if FRU's "Present" property is true, false otherwise.
     */
    bool isFruPresent(const std::string& i_objectPath) const noexcept;

    /**
     * @brief An API to get backup-restore config JSON of the system.
     *
     * API gets this file by prasing system config JSON file and reading
     * backupRestoreConfigPath tag.
     *
     * @return On success returns valid JSON object, otherwise returns empty
     * JSON object.
     *
     * Note: The caller of this API should verify, is received JSON object is
     * empty or not.
     */
    nlohmann::json getBackupRestoreCfgJsonObj() const noexcept;

    /**
     * @brief Prints the user options for fix system VPD command.
     *
     * @param[in] i_option - Option to use.
     */
    void printFixSystemVpdOption(
        const types::UserOption& i_option) const noexcept;

    /**
     * @brief API to update source and destination keyword's value.
     *
     * API fetches source and destination keyword's value,
     * for each keyword entries found in the input JSON object and updates the
     * JSON object. If the path(source / destination) in JSON object is
     * inventory object path, API sends the request to Inventory.Manager DBus
     * service. Otherwise if its a hardware path, API sends the request to
     * vpd-manager DBus service to get the keyword's value.
     *
     * @param[in,out] io_parsedJsonObj - Parsed JSON object.
     *
     * @return true on success, false in case of any error.
     */
    bool fetchKeywordInfo(nlohmann::json& io_parsedJsonObj) const noexcept;

    /**
     * @brief API to print system VPD keyword's information.
     *
     * The API prints source and destination keyword's information in the table
     * format, found in the JSON object.
     *
     * @param[in] i_parsedJsonObj - Parsed JSON object.
     */
    void printSystemVpd(const nlohmann::json& i_parsedJsonObj) const noexcept;

    /**
     * @brief API to update keyword's value.
     *
     * API iterates the given JSON object for all record-keyword pairs, if there
     * is any mismatch between source and destination keyword's value, API calls
     * the utils::writeKeyword API to update keyword's value.
     *
     * Note: writeKeyword API, internally updates primary, backup, redundant
     * EEPROM paths(if exists) with the given keyword's value.
     *
     * @param i_parsedJsonObj - Parsed JSON object.
     * @param i_useBackupData - Specifies whether to use source or destination
     * keyword's value to update the keyword's value.
     *
     * @return On success return 0, otherwise return -1.
     */
    int updateAllKeywords(const nlohmann::json& i_parsedJsonObj,
                          bool i_useBackupData) const noexcept;

    /**
     * @brief API to handle more option for fix system VPD command.
     *
     * @param i_parsedJsonObj - Parsed JSON object.
     *
     * @return On success return 0, otherwise return -1.
     */
    int handleMoreOption(const nlohmann::json& i_parsedJsonObj) const noexcept;

  public:
    /**
     * @brief Read keyword value.
     *
     * API to read VPD keyword's value from the given input path.
     * If the provided i_onHardware option is true, read keyword's value from
     * the hardware. Otherwise read keyword's value from DBus.
     *
     * @param[in] i_vpdPath - DBus object path or EEPROM path.
     * @param[in] i_recordName - Record name.
     * @param[in] i_keywordName - Keyword name.
     * @param[in] i_onHardware - True if i_vpdPath is EEPROM path, false
     * otherwise.
     * @param[in] i_fileToSave - File path to save keyword's value, if not given
     * result will redirect to a console.
     *
     * @return On success return 0, otherwise return -1.
     */
    int readKeyword(const std::string& i_vpdPath,
                    const std::string& i_recordName,
                    const std::string& i_keywordName, const bool i_onHardware,
                    const std::string& i_fileToSave = {});

    /**
     * @brief Dump the given inventory object in JSON format to console.
     *
     * For a given object path of a FRU, this API dumps the following properties
     * of the FRU in JSON format to console:
     * - Pretty Name, Location Code, Sub Model
     * - SN, PN, CC, FN, DR keywords under VINI record.
     * If the FRU's "Present" property is not true, the above properties are not
     * dumped to console.
     *
     * @param[in] i_fruPath - DBus object path.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int dumpObject(std::string i_fruPath) const noexcept;

    /**
     * @brief API to fix system VPD keywords.
     *
     * The API to fix the system VPD keywords. Mainly used when there
     * is a mismatch between the primary and backup(secondary) VPD. User can
     * choose option to update all primary keywords value with corresponding
     * backup keywords value or can choose primary keyword value to sync
     * secondary VPD. Otherwise, user can also interactively choose different
     * action for individual keyword.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int fixSystemVpd() const noexcept;

    /**
     * @brief Write keyword's value.
     *
     * API to update VPD keyword's value to the given input path.
     * If i_onHardware value in true, i_vpdPath is considered has hardware path
     * otherwise it will be considered as DBus object path.
     *
     * For provided DBus object path both primary path or secondary path will
     * get updated, also redundant EEPROM(if any) path with new keyword's value.
     *
     * In case of hardware path, only given hardware path gets updated with new
     * keyword’s value, any backup or redundant EEPROM (if exists) paths won't
     * get updated.
     *
     * @param[in] i_vpdPath - DBus object path or EEPROM path.
     * @param[in] i_recordName - Record name.
     * @param[in] i_keywordName - Keyword name.
     * @param[in] i_keywordValue - Keyword value.
     * @param[in] i_onHardware - True if i_vpdPath is EEPROM path, false
     * otherwise.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int writeKeyword(std::string i_vpdPath, const std::string& i_recordName,
                     const std::string& i_keywordName,
                     const std::string& i_keywordValue,
                     const bool i_onHardware) noexcept;

    /**
     * @brief Reset specific keywords on System VPD to default value.
     *
     * This API resets specific System VPD keywords to default value. The
     * keyword values are reset on:
     * 1. Primary EEPROM path.
     * 2. Secondary EEPROM path.
     * 3. D-Bus cache.
     * 4. Backup path.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int cleanSystemVpd() const noexcept;

    /**
     * @brief Dump all the inventory objects in JSON or table format to console.
     *
     * This API dumps specific properties of all the inventory objects to
     * console in JSON or table format to console. The inventory object paths
     * are extracted from PIM. For each object, the following properties are
     * dumped to console:
     * - Present property, Pretty Name, Location Code, Sub Model
     * - SN, PN, CC, FN, DR keywords under VINI record.
     * If the "Present" property of a FRU is false, the FRU is not dumped to
     * console.
     * FRUs whose object path end in "unit([0-9][0-9]?)" are also not dumped to
     * console.
     *
     * @param[in] i_dumpTable - Flag which specifies if the inventory should be
     * dumped in table format or not.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int dumpInventory(bool i_dumpTable = false) const noexcept;
};
} // namespace vpd
