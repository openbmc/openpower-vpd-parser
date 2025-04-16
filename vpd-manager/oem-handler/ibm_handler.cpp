#include "ibm_handler.hpp"

#include <utility/dbus_utility.hpp>
#include <utility/json_utility.hpp>

namespace vpd
{
IbmManager::IbmManager(
    std::shared_ptr<Worker>& o_worker,
    std::shared_ptr<BackupAndRestore>& o_backupAndRestoreObj,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_iFace,
    const std::shared_ptr<boost::asio::io_context>& i_ioCon,
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection) :
    m_worker(o_worker), m_backupAndRestoreObj(o_backupAndRestoreObj),
    m_interface(i_iFace), m_ioContext(i_ioCon),
    m_asioConnection(i_asioConnection)
{
    if (dbusUtility::isChassisPowerOn())
    {
        // At power on, less number of FRU(s) needs collection. we can scale
        // down the threads to reduce CPU utilization.
        m_worker = std::make_shared<Worker>(INVENTORY_JSON_DEFAULT,
                                            constants::VALUE_1);
    }
    else
    {
        // Initialize with default configuration
        m_worker = std::make_shared<Worker>(INVENTORY_JSON_DEFAULT);
    }

    // Set up minimal things that is needed before bus name is claimed.
    m_worker->performInitialSetup();

    // Get the system config JSON object
    m_sysCfgJsonObj = m_worker->getSysCfgJsonObj();

    if (!m_sysCfgJsonObj.empty() &&
        jsonUtility::isBackupAndRestoreRequired(m_sysCfgJsonObj))
    {
        try
        {
            m_backupAndRestoreObj =
                std::make_shared<BackupAndRestore>(m_sysCfgJsonObj);
        }
        catch (const std::exception& l_ex)
        {
            logging::logMessage("Back up and restore instantiation failed. {" +
                                std::string(l_ex.what()) + "}");

            EventLogger::createSyncPel(
                EventLogger::getErrorType(l_ex), types::SeverityType::Warning,
                __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        }
    }

    // set callback to detect any asset tag change
    // registerAssetTagChangeCallback();

    // set async timer to detect if system VPD is published on D-Bus.
    // SetTimerToDetectSVPDOnDbus();

    // set async timer to detect if VPD collection is done.
    // SetTimerToDetectVpdCollectionStatus();

    // Instantiate GpioMonitor class
    m_gpioMonitor =
        std::make_shared<GpioMonitor>(m_sysCfgJsonObj, m_worker, m_ioContext);
}
} // namespace vpd
