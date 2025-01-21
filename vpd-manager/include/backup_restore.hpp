#pragma once

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
    static void
        setBackupAndRestoreStatus(const BackupAndRestoreStatus& i_status);

  private:
    /**
     * @brief An API to handle backup and restore of IPZ type VPD.
     *
     * @param[in,out] io_srcVpdMap - Source VPD map.
     * @param[in,out] io_dstVpdMap - Destination VPD map.
     * @param[in] i_srcPath - Source EEPROM file path or inventory path.
     * @param[in] i_dstPath - Destination EEPROM file path or inventory path.
     *
     * @throw std::runtime_error
     */
    void backupAndRestoreIpzVpd(types::IPZVpdMap& io_srcVpdMap,
                                types::IPZVpdMap& io_dstVpdMap,
                                const std::string& i_srcPath,
                                const std::string& i_dstPath);

    // System JSON config JSON object.
    nlohmann::json m_sysCfgJsonObj{};

    // Backup and restore config JSON object.
    nlohmann::json m_backupAndRestoreCfgJsonObj{};

    // Backup and restore status.
    static BackupAndRestoreStatus m_backupAndRestoreStatus;
};

} // namespace vpd
