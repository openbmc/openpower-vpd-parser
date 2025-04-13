#pragma once
#include "worker.hpp"
#include "backup_restore.hpp"

namespace vpd
{
class IbmManager
{

public:
    /**
     * List of deleted methods.
     */
    IbmManager(const IbmManager&) = delete;
    IbmManager& operator=(const IbmManager&) = delete;
    IbmManager(IbmManager&&) = delete;

    /**
     * @brief Constructor.
     */
    IbmManager(std::shared_ptr<Worker>& o_worker, 
        std::shared_ptr<BackupAndRestore>& o_backupAndRestoreObj);

private:
    
    /**
     * @brief API to set timer to detect system VPD over D-Bus.
     *
     * System VPD is required before bus name for VPD-Manager is claimed. Once
     * system VPD is published, VPD for other FRUs should be collected. This API
     * detects id system VPD is already published on D-Bus and based on that
     * triggers VPD collection for rest of the FRUs.
     *
     * Note: Throws exception in case of any failure. Needs to be handled by the
     * caller.
     */
    void SetTimerToDetectSVPDOnDbus();

    /**
     * @brief Set timer to detect and set VPD collection status for the system.
     *
     * Collection of FRU VPD is triggered in a separate thread. Resulting in
     * multiple threads at  a given time. The API creates a timer which on
     * regular interval will check if all the threads were collected back and
     * sets the status of the VPD collection for the system accordingly.
     *
     * @throw std::runtime_error
     */
    void SetTimerToDetectVpdCollectionStatus();

    /**
     * @brief API to register callback for "AssetTag" property change.
     */
    void registerAssetTagChangeCallback();

    /**
     * @brief Callback API to be triggered on "AssetTag" property change.
     *
     * @param[in] i_msg - The callback message.
     */
    void processAssetTagChangeCallback(sdbusplus::message_t& i_msg);

    /**
     * @brief API to process VPD collection thread failed EEPROMs.
     */
    void processFailedEeproms();

    /**
     * @brief API to check and update PowerVS VPD.
     *
     * The API will read the existing data from the DBus and if found
     * different than what has been read from JSON, it will update the VPD with
     * JSON data on hardware and DBus both.
     *
     * @param[in] i_powerVsJsonObj - PowerVS JSON object.
     * @param[out] o_failedPathList - List of path failed to update.
     */
    void checkAndUpdatePowerVsVpd(const nlohmann::json& i_powerVsJsonObj,
                                std::vector<std::string>& o_failedPathList);

    /**
     * @brief API to handle configuration w.r.t. PowerVS systems.
     *
     * Some FRUs VPD is specific to powerVS system. The API detects the
     * powerVS configuration and updates the VPD accordingly.
     */
    void ConfigurePowerVsSystem();
    
    // Shared pointer to worker class
    std::shared_ptr<Worker>& m_worker;

    // Shared pointer to backup and restore class
    std::shared_ptr<BackupAndRestore>& m_backupAndRestoreObj;

    // Variable to hold current collection status
    std::string m_vpdCollectionStatus = "NotStarted";
};
}//namespace vpd