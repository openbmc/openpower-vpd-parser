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
     * @param[in] sysCfgJsonObj - System config JSON object.
     *
     * @throw std::runtime_error in case constructor failure.
     */
    BackupAndRestore(const nlohmann::json& sysCfgJsonObj);

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
     * @param[in] status - Status to set.
     */
    static void setBackupAndRestoreStatus(const BackupAndRestoreStatus& status);

    /**
     * @brief An API to update keyword's value on primary or backup path.
     *
     * Updates the keyword's value based on the following,
     * 1. If provided fruPath is primary(source) path in the backup restore
     * config JSON, then API updates VPD on the backup(destination) path.
     * 2. If fruPath is backup path, then API updates the VPD on the
     * primary path.
     *
     * Note: The above condition is only valid,
     * 1. If system's primary & backup VPD is on EEPROM path(and should be found
     * in the backup and restore config JSON).
     * 2. If the input record and keyword are found in the backup and restore
     * config JSON.
     *
     * @param[in] fruPath - EEPROM path of the FRU.
     * @param[in] paramsToWriteData - Input details.
     *
     * @return On success returns number of bytes written, -1 on failure.
     */
    int updateKeywordOnPrimaryOrBackupPath(
        const std::string& fruPath,
        const types::WriteVpdParams& paramsToWriteData) const noexcept;

  private:
    /**
     * @brief An API to handle backup and restore of IPZ type VPD.
     *
     * @param[in,out] srcVpdMap - Source VPD map.
     * @param[in,out] dstVpdMap - Destination VPD map.
     *
     * @throw std::runtime_error
     */
    void backupAndRestoreIpzVpd(types::IPZVpdMap& srcVpdMap,
                                types::IPZVpdMap& dstVpdMap);

    /**
     * @brief Get source and destination dbus service name.
     *
     * This API extracts the source and destination Dbus service names from the
     * system configuration JSON using the inventory paths stored in
     * srcInvPath and dstInvPath.
     *
     * @param[out] srcServiceName - Source service name.
     * @param[out] dstServiceName - Destination service name.
     *
     * @return A tuple containing the source and destination Dbus service name
     * on successful retrieval, or an empty tuple otherwise..
     */
    std::tuple<std::string, std::string> getSrcAndDstServiceName()
        const noexcept;

    /**
     * @brief Retrieve EEPROM and inventory object paths.
     *
     * This API retrieves the EEPROM and inventory object paths for the given
     * location (source or destination) from the backup and restore
     * configuration and the system configuration JSONs.
     *
     * @param[in] location - Source or destination location.
     *
     * @return A tuple containing the EEPROM and inventory paths on successful
     *         retrieval, or an empty tuple otherwise.
     */
    types::EepromInventoryPaths getFruAndInvPaths(
        const std::string& location) const noexcept;

    /**
     * @brief Extract and validate IPZ type record details.
     *
     * This API extracts the source and destination record name, keyword name,
     * and default value from the input JSON object. It also validates that the
     * extracted source and destination records are present in the provided VPD
     * maps only when the maps are provided.
     *
     * @param[in] aRecordKwInfo - Json object containing record and keyword
     * details.
     * @param[out] srcDstRecordKeywordInfo - Tuple containing (source record
     * name, source keyword name, destination record name, destination keyword
     * name, default value).
     * @param[in] srcVpdMap - Optional source IPZ VPD map.
     * @param[in] dstVpdMap - Optional destination IPZ VPD map.
     *
     * @return true if record details are successfully extracted and validated,
     *         false otherwise.
     */
    bool extractAndValidateIpzRecordDetails(
        const auto& aRecordKwInfo,
        types::SrcDstRecordDetails srcDstRecordKeywordInfo,
        const std::optional<types::IPZVpdMap>& srcVpdMap,
        const std::optional<types::IPZVpdMap>& dstVpdMap) const noexcept;

    /**
     * @brief Retrieve the binary and string values of a keyword for IPZ type.
     *
     * This API returns a tuple containing the binary and string values for the
     * given record and keyword. If the input VPD map is not empty, the keyword
     * value is extracted from the map; otherwise, the keyword value is
     * retrieved through a D-Bus query.
     *
     * @param[in] recordKwName - Tuple of record and keyword name.
     * @param[in] vpdMap - IPZ VPD map.
     * @param[in] serviceName - Dbus service name.
     *
     * @return A tuple containing the keyword's binary value and its string
     *         representation on success, or an empty tuple on failure.
     */
    types::BinaryStringKwValuePair getBinaryAndStrIpzKwValue(
        const types::IpzType& recordKwName, const types::IPZVpdMap& vpdMap,
        const std::string& serviceName) const noexcept;

    /**
     * @brief Synchronize a keyword value to EEPROM for IPZ type.
     *
     * This API updates the specified record's keyword value on the given
     * EEPROM. On success, it updates the corresponding string value in the
     * provided VPD map if not empty.
     *
     * @param[in] fruPath - EEPROM path.
     * @param[in] recordKwName - Tuple of record and keyword name.
     * @param[in] binaryStrValue -  Tuple of Keyword value in Binary and
     * string format.
     * @param[out] vpdMap - IPZ VPD map.
     */
    void syncIpzData(const std::string& fruPath,
                     const types::IpzType& recordKwName,
                     const types::BinaryStringKwValuePair& binaryStrValue,
                     types::IPZVpdMap& vpdMap) const noexcept;

    /* @brief API to check if the JSON parsed is valid.
     *
     * The API check for required details in the parsed JSON and flags any error
     * if not found with mandatory tags to process backup and restore.
     *
     * @return True if valid, false otherwise.
     */
    bool isJsonValid();

    // System JSON config JSON object.
    nlohmann::json sysCfgJsonObj{};

    // Backup and restore config JSON object.
    nlohmann::json backupAndRestoreCfgJsonObj{};

    // Backup and restore status.
    static BackupAndRestoreStatus backupAndRestoreStatus;

    // Shared pointer to Logger object
    std::shared_ptr<Logger> logger;

    // Source path
    std::string srcFruPath{};
    std::string srcInvPath{};

    // Destination path
    std::string dstFruPath{};
    std::string dstInvPath{};
};

} // namespace vpd
