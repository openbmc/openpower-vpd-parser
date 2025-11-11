#include "config.h"

#include "ibm_handler.hpp"

#include "listener.hpp"
#include "parser.hpp"

#include <utility/common_utility.hpp>
#include <utility/dbus_utility.hpp>
#include <utility/json_utility.hpp>
#include <utility/vpd_specific_utility.hpp>

namespace vpd
{
IbmHandler::IbmHandler(
    std::shared_ptr<Worker>& o_worker,
    std::shared_ptr<BackupAndRestore>& o_backupAndRestoreObj,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_iFace,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_progressiFace,
    const std::shared_ptr<boost::asio::io_context>& i_ioCon,
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection) :
    m_worker(o_worker), m_backupAndRestoreObj(o_backupAndRestoreObj),
    m_interface(i_iFace), m_progressInterface(i_progressiFace),
    m_ioContext(i_ioCon), m_asioConnection(i_asioConnection),
    m_logger(Logger::getLoggerInstance())
{
    uint16_t l_errCode{0};

    // check VPD collection mode
    const auto l_vpdCollectionMode =
        commonUtility::isFieldModeEnabled()
            ? types::VpdCollectionMode::DEFAULT_MODE
            : commonUtility::getVpdCollectionMode(l_errCode);

    if (l_errCode)
    {
        m_logger->logMessage(
            "Error while trying to read VPD collection mode: " +
            commonUtility::getErrCodeMsg(l_errCode));
    }

    if (dbusUtility::isChassisPowerOn())
    {
        // At power on, less number of FRU(s) needs collection. we can scale
        // down the threads to reduce CPU utilization.
        m_worker = std::make_shared<Worker>(
            INVENTORY_JSON_DEFAULT, constants::VALUE_1, l_vpdCollectionMode);
    }
    else
    {
        // Initialize with default configuration
        m_worker = std::make_shared<Worker>(INVENTORY_JSON_DEFAULT,
                                            constants::MAX_THREADS,
                                            l_vpdCollectionMode);
    }

    // Set up minimal things that is needed before bus name is claimed.
    performInitialSetup();

    if (!m_sysCfgJsonObj.empty() &&
        jsonUtility::isBackupAndRestoreRequired(m_sysCfgJsonObj, l_errCode))
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
    else if (l_errCode)
    {
        logging::logMessage(
            "Failed to check if backup & restore required. Error : " +
            commonUtility::getErrCodeMsg(l_errCode));
    }

    // Instantiate Listener object
    m_eventListener = std::make_shared<Listener>(m_worker, m_asioConnection);
    m_eventListener->registerAssetTagChangeCallback();
    m_eventListener->registerHostStateChangeCallback();
    m_eventListener->registerPresenceChangeCallback();

    // Instantiate GpioMonitor class
    m_gpioMonitor =
        std::make_shared<GpioMonitor>(m_sysCfgJsonObj, m_worker, m_ioContext);
}

void IbmHandler::SetTimerToDetectVpdCollectionStatus()
{
    // Keeping max retry for 2 minutes. TODO: Make it configurable based on
    // system type.
    static constexpr auto MAX_RETRY = 12;

    static boost::asio::steady_timer l_timer(*m_ioContext);
    static uint8_t l_timerRetry = 0;

    auto l_asyncCancelled = l_timer.expires_after(std::chrono::seconds(10));

    (l_asyncCancelled == 0)
        ? logging::logMessage("Collection Timer started")
        : logging::logMessage("Collection Timer re-started");

    l_timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            throw std::runtime_error(
                "Timer to detect thread collection status was aborted");
        }

        if (ec)
        {
            throw std::runtime_error(
                "Timer to detect thread collection failed");
        }

        if (m_worker->isAllFruCollectionDone())
        {
            // cancel the timer
            l_timer.cancel();
            processFailedEeproms();

            // update VPD for powerVS system.
            ConfigurePowerVsSystem();

            m_logger->logMessage("m_worker->isSystemVPDOnDBus() completed",PlaceHolder::COLLECTION);
            m_progressInterface->set_property(
                "Status", std::string(constants::vpdCollectionCompleted));

            if (m_backupAndRestoreObj)
            {
                m_backupAndRestoreObj->backupAndRestore();
            }

            if (m_eventListener)
            {
                m_eventListener->registerCorrPropCallBack();
            }
#ifdef ENABLE_FILE_LOGGING
            // terminate collection logger
            m_logger->terminateVpdCollectionLogging();
#endif
        }
        else
        {
            auto l_threadCount = m_worker->getActiveThreadCount();
            if (l_timerRetry == MAX_RETRY)
            {
                l_timer.cancel();
                m_logger->logMessage("Taking too long. Active thread = " +
                                    std::to_string(l_threadCount), PlaceHolder::COLLECTION);
#ifdef ENABLE_FILE_LOGGING
                // terminate collection logger
                m_logger->terminateVpdCollectionLogging();
#endif
            }
            else
            {
                l_timerRetry++;
                m_logger->logMessage("Collection is in progress for [" +
                                    std::to_string(l_threadCount) + "] FRUs.", PlaceHolder::COLLECTION);

                SetTimerToDetectVpdCollectionStatus();
            }
        }
    });
}

void IbmHandler::checkAndUpdatePowerVsVpd(
    const nlohmann::json& i_powerVsJsonObj,
    std::vector<std::string>& o_failedPathList)
{
    for (const auto& [l_fruPath, l_recJson] : i_powerVsJsonObj.items())
    {
        nlohmann::json l_sysCfgJsonObj{};
        if (m_worker.get() != nullptr)
        {
            l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();
        }

        // The utility method will handle emty JSON case. No explicit
        // handling required here.
        uint16_t l_errCode = 0;
        auto l_inventoryPath = jsonUtility::getInventoryObjPathFromJson(
            l_sysCfgJsonObj, l_fruPath, l_errCode);

        // Mark it as failed if inventory path not found in JSON.
        if (l_inventoryPath.empty())
        {
            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to get inventory object path from JSON for FRU [" +
                    l_fruPath +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode), PlaceHolder::COLLECTION);
            }

            o_failedPathList.push_back(l_fruPath);
            continue;
        }

        // check if the FRU is present
        if (!dbusUtility::isInventoryPresent(l_inventoryPath))
        {
            m_logger->logMessage(
                "Inventory not present, skip updating part number. Path: " +
                l_inventoryPath, PlaceHolder::COLLECTION);
            continue;
        }

        // check if the FRU needs CCIN check before updating PN.
        if (l_recJson.contains("CCIN"))
        {
            const auto& l_ccinFromDbus =
                vpdSpecificUtility::getCcinFromDbus(l_inventoryPath, l_errCode);

            // Not an ideal situation as CCIN can't be empty.
            if (l_ccinFromDbus.empty())
            {
                if (l_errCode)
                {
                    m_logger->logMessage(
                        "Failed to get CCIN value from DBus, error : " +
                        commonUtility::getErrCodeMsg(l_errCode), PlaceHolder::COLLECTION);
                }

                o_failedPathList.push_back(l_fruPath);
                continue;
            }

            std::vector<std::string> l_ccinListFromJson = l_recJson["CCIN"];

            if (find(l_ccinListFromJson.begin(), l_ccinListFromJson.end(),
                     l_ccinFromDbus) == l_ccinListFromJson.end())
            {
                // Don't update PN in this case.
                continue;
            }
        }

        for (const auto& [l_recordName, l_kwdJson] : l_recJson.items())
        {
            // Record name can't be CCIN, skip processing as it is there for PN
            // update based on CCIN check.
            if (l_recordName == constants::kwdCCIN)
            {
                continue;
            }

            for (const auto& [l_kwdName, l_kwdValue] : l_kwdJson.items())
            {
                // Is value of type array.
                if (!l_kwdValue.is_array())
                {
                    o_failedPathList.push_back(l_fruPath);
                    continue;
                }

                // Get current FRU Part number.
                auto l_retVal = dbusUtility::readDbusProperty(
                    constants::pimServiceName, l_inventoryPath,
                    constants::viniInf, constants::kwdFN);

                auto l_ptrToFn = std::get_if<types::BinaryVector>(&l_retVal);

                if (!l_ptrToFn)
                {
                    o_failedPathList.push_back(l_fruPath);
                    continue;
                }

                types::BinaryVector l_binaryKwdValue =
                    l_kwdValue.get<types::BinaryVector>();
                if (l_binaryKwdValue == (*l_ptrToFn))
                {
                    continue;
                }

                // Update part number only if required.
                std::shared_ptr<Parser> l_parserObj =
                    std::make_shared<Parser>(l_fruPath, l_sysCfgJsonObj);
                if (l_parserObj->updateVpdKeyword(std::make_tuple(
                        l_recordName, l_kwdName, l_binaryKwdValue)) ==
                    constants::FAILURE)
                {
                    o_failedPathList.push_back(l_fruPath);
                    continue;
                }

                // update the Asset interface Spare part number explicitly.
                if (!dbusUtility::callPIM(types::ObjectMap{
                        {l_inventoryPath,
                         {{constants::assetInf,
                           {{"SparePartNumber",
                             std::string(l_binaryKwdValue.begin(),
                                         l_binaryKwdValue.end())}}}}}}))
                {
                    m_logger->logMessage(
                        "Updating Spare Part Number under Asset interface failed for path [" +
                        l_inventoryPath + "]", PlaceHolder::COLLECTION);
                }

                // Just needed for logging.
                std::string l_initialPartNum((*l_ptrToFn).begin(),
                                             (*l_ptrToFn).end());
                std::string l_finalPartNum(l_binaryKwdValue.begin(),
                                           l_binaryKwdValue.end());
                m_logger->logMessage(
                    "FRU Part number updated for path [" + l_inventoryPath +
                    "]" + "From [" + l_initialPartNum + "]" + " to [" +
                    l_finalPartNum + "]", PlaceHolder::COLLECTION);
            }
        }
    }
}

void IbmHandler::ConfigurePowerVsSystem()
{
    std::vector<std::string> l_failedPathList;
    try
    {
        types::BinaryVector l_imValue = dbusUtility::getImFromDbus();
        if (l_imValue.empty())
        {
            throw DbusException("Invalid IM value read from Dbus");
        }

        uint16_t l_errCode = 0;
        if (!vpdSpecificUtility::isPowerVsConfiguration(l_imValue, l_errCode))
        {
            // TODO: Should booting be blocked in case of some
            // misconfigurations?
            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to check if the system is powerVs Configuration, error : " +
                    commonUtility::getErrCodeMsg(l_errCode), PlaceHolder::COLLECTION);
            }

            return;
        }

        const nlohmann::json& l_powerVsJsonObj =
            jsonUtility::getPowerVsJson(l_imValue, l_errCode);

        if (l_powerVsJsonObj.empty())
        {
            throw std::runtime_error("PowerVS Json not found. Error : " +
                                     commonUtility::getErrCodeMsg(l_errCode));
        }

        checkAndUpdatePowerVsVpd(l_powerVsJsonObj, l_failedPathList);

        if (!l_failedPathList.empty())
        {
            throw std::runtime_error(
                "Part number update failed for following paths: ");
        }
    }
    catch (const std::exception& l_ex)
    {
        // TODO log appropriate PEL
    }
}

void IbmHandler::processFailedEeproms()
{
    if (m_worker.get() != nullptr)
    {
        // TODO:
        // - iterate through list of EEPROMs for which thread creation has
        // failed
        // - For each failed EEPROM, trigger VPD collection
        m_worker->getFailedEepromPaths().clear();
    }
}

void IbmHandler::enableMuxChips()
{
    if (m_sysCfgJsonObj.empty())
    {
        // config JSON should not be empty at this point of execution.
        throw std::runtime_error("Config JSON is empty. Can't enable muxes");
        return;
    }

    if (!m_sysCfgJsonObj.contains("muxes"))
    {
        logging::logMessage("No mux defined for the system in config JSON");
        return;
    }

    // iterate over each MUX detail and enable them.
    for (const auto& item : m_sysCfgJsonObj["muxes"])
    {
        if (item.contains("holdidlepath"))
        {
            std::string cmd = "echo 0 > ";
            cmd += item["holdidlepath"];

            logging::logMessage("Enabling mux with command = " + cmd);

            commonUtility::executeCmd(cmd);
            continue;
        }

        logging::logMessage(
            "Mux Entry does not have hold idle path. Can't enable the mux");
    }
}

void IbmHandler::performInitialSetup()
{
    try
    {
        if (m_worker.get() == nullptr)
        {
            throw std::runtime_error(
                "Worker object not found. Can't perform initial setup.");
        }

        m_sysCfgJsonObj = m_worker->getSysCfgJsonObj();
        if (!dbusUtility::isChassisPowerOn())
        {
            m_worker->setDeviceTreeAndJson();

            // Since the above function setDeviceTreeAndJson can change the json
            // which is used, we would need to reacquire the json object again
            // here.
            m_sysCfgJsonObj = m_worker->getSysCfgJsonObj();
        }

        // Update BMC postion for RBMC prototype system
        // Ignore BMC position update in case of any error
        uint16_t l_errCode = 0;
        if (isRbmcPrototypeSystem(l_errCode))
        {
            size_t l_bmcPosition = std::numeric_limits<size_t>::max();
            checkAndUpdateBmcPosition(l_bmcPosition);

            if (dbusUtility::callPIM(types::ObjectMap{
                    {sdbusplus::message::object_path(constants::systemInvPath),
                     {{constants::rbmcPositionInterface,
                       {{"Position", l_bmcPosition}}}}}}))
            {
                m_logger->logMessage(
                    "Updating BMC position failed for path [" +
                    std::string(constants::systemInvPath) +
                    "], bmc position: " + std::to_string(l_bmcPosition));

                // ToDo: Check is PEL required
            }
        }
        else if (l_errCode != 0)
        {
            m_logger->logMessage(
                "Unable to determine whether system is RBMC system or not, reason: " +
                commonUtility::getErrCodeMsg(l_errCode));
        }

        // Enable all mux which are used for connecting to the i2c on the
        // pcie slots for pcie cards. These are not enabled by kernel due to
        // an issue seen with Castello cards, where the i2c line hangs on a
        // probe.
        enableMuxChips();

        // Nothing needs to be done. Service restarted or BMC re-booted for
        // some reason at system power on.
    }
    catch (const std::exception& l_ex)
    {
        m_worker->setCollectionStatusProperty(SYSTEM_VPD_FILE_PATH,
                                              constants::vpdCollectionFailed);
        // Any issue in system's inital set up is handled in this catch. Error
        // will not propogate to manager.
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Critical,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

void IbmHandler::collectAllFruVpd()
{
#ifdef ENABLE_FILE_LOGGING
    // initialize VPD collection logger
    m_logger->initiateVpdCollectionLogging();
#endif

    // Setting status to "InProgress", before trigeering VPD collection.
    m_progressInterface->set_property(
        "Status", std::string(constants::vpdCollectionInProgress));
    m_worker->collectFrusFromJson();
    SetTimerToDetectVpdCollectionStatus();
}

bool IbmHandler::isRbmcPrototypeSystem(uint16_t& o_errCode) const noexcept
{
    types::BinaryVector l_imValue = dbusUtility::getImFromDbus();
    if (l_imValue.empty())
    {
        o_errCode = error_code::DBUS_FAILURE;
        return false;
    }

    if (constants::rbmcPrototypeSystemImValue == l_imValue)
    {
        return true;
    }

    return false;
}

void IbmHandler::checkAndUpdateBmcPosition(size_t& o_bmcPosition) const noexcept
{
    if (m_worker.get() == nullptr)
    {
        m_logger->logMessage("Worker object not found");
        return;
    }

    const nlohmann::json& l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();
    if (l_sysCfgJsonObj.empty())
    {
        m_logger->logMessage(
            "System config JSON is empty, unable to find BMC position");
        return;
    }

    uint16_t l_errCode = 0;
    std::string l_motherboardEepromPath = jsonUtility::getFruPathFromJson(
        l_sysCfgJsonObj, constants::systemVpdInvPath, l_errCode);

    if (!l_motherboardEepromPath.empty())
    {
        o_bmcPosition = constants::VALUE_1;
        std::error_code l_ec;
        if (std::filesystem::exists(l_motherboardEepromPath, l_ec))
        {
            o_bmcPosition = constants::VALUE_0;
        }
    }
    else if (l_errCode)
    {
        m_logger->logMessage("Unable to determine BMC position, reason: " +
                             commonUtility::getErrCodeMsg(l_errCode));
    }
    else
    {
        m_logger->logMessage("Unable to determine BMC position, as FRU path[" +
                             std::string(constants::systemVpdInvPath) +
                             "], not found in the system config JSON.");
    }
}
} // namespace vpd
