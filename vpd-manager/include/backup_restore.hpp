#pragma once

#include "logger.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>

#include <tuple>

namespace vpd
{

// Backup and restore operation status.
enum class BackupAndRestoreStatus : uint8_t
{
    NotStarted,
    Invoked,
    Completed
};

/**
 * @brief class to implement backup and restore VPD.
 *
 */

class BackupAndRestore
{
  public:
    // delete functions
    BackupAndRestore() = delete;
    BackupAndRestore(const BackupAndRestore&) = delete;
    BackupAndRestore& operator=(const BackupAndRestore&) = delete;
    BackupAndRestore(BackupAndRestore&&) = delete;
    BackupAndRestore& operator=(BackupAndRestore&&) = delete;

    /**
     * @brief Constructor.
     *
     * @param[in] i_sysCfgJsonObj - System config JSON object.
     *
     * @throw std::runtime_error in case constructor failure.
     */
    BackupAndRestore(const nlohmann::json& i_sysCfgJsonObj);

    /**
     * @brief Default destructor.
     */
    ~BackupAndRestore() = default;

    /**
     * @brief An API to backup and restore VPD.
     *
     * Note: This API works on the keywords declared in the backup and restore
     * config JSON. Restore or backup action could be triggered for each
     * keyword, based on the keyword's value present in the source and
     * destination keyword.
     *
     * Restore source keyword's value with destination keyword's value,
     * when source keyword has default value but
     * destination's keyword has non default value.
     *
     * Backup the source keyword value to the destination's keyword's value,
     * when source keyword has non default value but
     * destination's keyword has default value.
     *
     * @return Tuple of updated source and destination VPD map variant.
     */
    std::tuple<types::VPDMapVariant, types::VPDMapVariant> backupAndRestore();

    /**
     * @brief An API to set backup and restore status.
     *
     * @param[in] i_status - Status to set.
     */
    static void setBackupAndRestoreStatus(
        const BackupAndRestoreStatus& i_status);

    /**
     * @brief An API to update keyword's value on primary or backup path.
     *
     * Updates the keyword's value based on the following,
     * 1. If provided i_fruPath is primary(source) path in the backup restore
     * config JSON, then API updates VPD on the backup(destination) path.
     * 2. If i_fruPath is backup path, then API updates the VPD on the
     * primary path.
     *
     * Note: The above condition is only valid,
     * 1. If system's primary & backup VPD is on EEPROM path(and should be found
     * in the backup and restore config JSON).
     * 2. If the input record and keyword are found in the backup and restore
     * config JSON.
     *
     * @param[in] i_fruPath - EEPROM path of the FRU.
     * @param[in] i_paramsToWriteData - Input details.
     *
     * @return On success returns number of bytes written, -1 on failure.
     */
    int updateKeywordOnPrimaryOrBackupPath(
        const std::string& i_fruPath,
        const types::WriteVpdParams& i_paramsToWriteData) const noexcept;

  private:
    /**
     * @brief An API to handle backup and restore of IPZ type VPD.
     *
     * @param[in,out] io_srcVpdMap - Source VPD map.
     * @param[in,out] io_dstVpdMap - Destination VPD map.
     *
     * @throw std::runtime_error
     */
    void backupAndRestoreIpzVpd(types::IPZVpdMap& io_srcVpdMap,
                                types::IPZVpdMap& io_dstVpdMap);

    /**
     * @brief Get source and destination dbus service name.
     *
     * @param[out] o_errCode - Error code set on failure.
     * @param[out] o_srcServiceName - Source service name.
     * @param[out] o_dstServiceName - Destination service name.
     *
     * @return true if source and destination service name are successfully
     * retrieved, false otherwise.
     */

    bool getSrcAndDstServiceName(uint16_t& o_errCode,
                                 std::string& o_srcServiceName,
                                 std::string& o_dstServiceName) const noexcept;

    /**
     * @brief Retrieve EEPROM and inventory object paths.
     *
     * This API retrieves the EEPROM and inventory object paths for the given
     * location (source or destination) from the backup and restore
     * configuration and the system configuration JSONs.
     *
     * @param[in]  i_location - Source or destination location.
     * @param[out] o_errCode - Error code set on failure.
     *
     * @return A tuple containing the EEPROM and inventory paths on successful
     *         retrieval, or an empty tuple otherwise.
     */
    types::EepromInventoryPaths getFruAndInvPaths(
        const std::string& i_location, uint16_t& o_errCode) const noexcept;

    /**
     * @brief Extract and validate record details.
     *
     * This API extracts the source and destination record name, keyword name,
     * and default value from the input JSON object. It also validates that the
     * extracted source and destination records are present in the provided VPD
     * maps when they are not empty.
     *
     * @param[in] i_aRecordKwInfo - Json object containing record and keyword
     * details.
     * @param[out] o_recordKeywordTuple - Tuple containing (source record name,
     * source keyword name, destination record name, destination keyword name,
     * default value).
     * @param[in] i_srcVpdMap - Optional source IPZ VPD map.
     * @param[in] i_dstVpdMap - Optional destination IPZ VPD map.
     *
     * @return true if record details are successfully extracted and validated,
     *         false otherwise.
     */
    bool extractAndFindRecordInMap(
        const auto& i_aRecordKwInfo,
        std::tuple<std::string&, std::string&, std::string&, std::string&,
                   types::BinaryVector&>
            o_recordKeywordTuple,
        const std::optional<types::IPZVpdMap>& i_srcVpdMap,
        const std::optional<types::IPZVpdMap>& i_dstVpdMap) const noexcept;

    /**
     * @brief Retrieve the binary and string values of a keyword.
     *
     * This API returns a tuple containing the binary and string values for the
     * given record and keyword. If the input VPD map is not empty, the keyword
     * value is extracted from the map; otherwise, the keyword value is
     * retrieved through a D-Bus query.
     *
     * @param[in] i_recordName -  Record name.
     * @param[in] i_keywordName - Keyword name.
     * @param[in] i_vpdMap - IPZ VPD map.
     * @param[in] i_serviceName - Dbus service name.
     * @param[out] o_errCode - Error code set on failure.
     *
     * @return A tuple containing the keyword's binary value and its string
     *         representation on success, or an empty tuple on failure.
     */
    std::tuple<types::BinaryVector, std::string> getBinaryAndStrKwValue(
        const std::string& i_recordName, const std::string& i_keywordName,
        const types::IPZVpdMap& i_vpdMap, const std::string& i_serviceName,
        uint16_t& o_errCode) const noexcept;

    /**
     * @brief Synchronize a keyword value to EEPROM.
     *
     * This API updates the specified record's keyword value on the given
     * EEPROM. On success, it updates the corresponding string value in the
     * provided VPD map if not empty.
     *
     * @param[in] i_fruPath - EEPROM path.
     * @param[in] i_recordName - Record name.
     * @param[in] i_keywordName - Keyword name.
     * @param[in] i_binaryValue - Keyword value in Binary format.
     * @param[in] i_strValue - Keyword value in string format.
     * @param[out] o_vpdMap - IPZ VPD map.
     */
    void syncData(const std::string& i_fruPath, const std::string& i_recordName,
                  const std::string& i_keywordName,
                  const types::BinaryVector& i_binaryValue,
                  const std::string& i_strValue,
                  types::IPZVpdMap& o_vpdMap) const noexcept;

    // System JSON config JSON object.
    nlohmann::json m_sysCfgJsonObj{};

    // Backup and restore config JSON object.
    nlohmann::json m_backupAndRestoreCfgJsonObj{};

    // Backup and restore status.
    static BackupAndRestoreStatus m_backupAndRestoreStatus;

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger;

    // Source path
    std::string m_srcFruPath{};
    std::string m_srcInvPath{};

    // Destination path
    std::string m_dstFruPath{};
    std::string m_dstInvPath{};
};

} // namespace vpd
