#include "config.h"

#include "ibm_handler.hpp"

#include "configuration.hpp"
#include "listener.hpp"
#include "logger.hpp"
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
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection,
    const types::VpdCollectionMode& i_vpdCollectionMode) :
    m_worker(o_worker), m_backupAndRestoreObj(o_backupAndRestoreObj),
    m_interface(i_iFace), m_progressInterface(i_progressiFace),
    m_ioContext(i_ioCon), m_asioConnection(i_asioConnection),
    m_logger(Logger::getLoggerInstance()),
    m_vpdCollectionMode(i_vpdCollectionMode)
{
    try
    {
        // check if symlink is present
        isSymlinkPresent();

        // Set up minimal things that is needed before bus name is claimed.
        performInitialSetup();

        // Init back up and restore.
        initBackupAndRestore();

        // Instantiate Listener objects
        initEventListeners();

        // Instantiate GpioMonitor class
        m_gpioMonitor = std::make_shared<GpioMonitor>(m_sysCfgJsonObj, m_worker,
                                                      m_ioContext);
    }
    catch (const std::exception& l_ec)
    {
        // PEL must have been logged if the code is at this point. So no need to
        // log again. Let the service continue to execute.
        m_logger->logMessage("IBM Handler instantiation failed. Reason: " +
                             std::string(l_ec.what()));
    }
}

void IbmHandler::isSymlinkPresent() noexcept
{
    // Check if symlink is already there to confirm fresh boot/factory reset.
    std::error_code l_ec;
    if (!std::filesystem::exists(INVENTORY_JSON_SYM_LINK, l_ec))
    {
        if (l_ec)
        {
            m_logger->logMessage(
                "Error reading symlink location. Reason: " + l_ec.message());
        }

        if (dbusUtility::isChassisPowerOn())
        {
            // Predictive PEL logged. Symlink can't go missing while chassis
            // is on as system VPD will not get processed in chassis on state.
            types::PelInfoTuple l_pel(
                types::ErrorType::FirmwareError, types::SeverityType::Warning,
                0, std::nullopt, std::nullopt, std::nullopt, std::nullopt);

            m_logger->logMessage(
                std::string(
                    "Error reading config JSON symlink in chassis on state."),
                PlaceHolder::PEL, &l_pel);
        }
        return;
    }

    m_logger->logMessage("Sym Link present.");

    // update JSON path to symlink path.
    m_configJsonPath = INVENTORY_JSON_SYM_LINK;
    m_isSymlinkPresent = true;
}

void IbmHandler::initWorker()
{
    try
    {
        // At power on, less number of FRU(s) needs collection. Hence defaulted
        // to 1.
        uint8_t l_threadCount = constants::VALUE_1;
        if (!dbusUtility::isChassisPowerOn())
        {
            // TODO: Can be configured from recipe? Check.
            l_threadCount = constants::MAX_THREADS;
        }

        // Initialize worker with required parameters.
        m_worker = std::make_shared<Worker>(m_configJsonPath, l_threadCount,
                                            m_vpdCollectionMode);
    }
    catch (const std::exception& l_ex)
    {
        // Critical PEL logged as collection can't progress without worker
        // object.
        const types::PelInfoTuple l_pel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Critical, 0,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);

        m_logger->logMessage(
            std::string("Exception while creating worker object") +
                EventLogger::getErrorMsg(l_ex),
            PlaceHolder::PEL, &l_pel);

        // Throwing error back to avoid any further processing.
        throw std::runtime_error(
            std::string("Exception while creating worker object") +
            EventLogger::getErrorMsg(l_ex));
    }
}

void IbmHandler::initBackupAndRestore() noexcept
{
    try
    {
        uint16_t l_errCode = 0;

        // If the object is already there, implies back up and restore took
        // place in inital set up flow.
        if ((m_backupAndRestoreObj == nullptr))
        {
            if (m_sysCfgJsonObj.empty())
            {
                // Throwing as sysconfig JSON empty is not expected at this
                // point of execution and also not having backup and restore
                // object will effect system VPD sync.
                throw std::runtime_error(
                    "sysconfig JSON found empty while initializing back up and restore onject. JSON path: " +
                    m_configJsonPath);
            }

            if (!jsonUtility::isBackupAndRestoreRequired(m_sysCfgJsonObj,
                                                         l_errCode))
            {
                if (l_errCode)
                {
                    // Throwing as setting of error code confirms that back up
                    // and restore object will not get initialized. This will
                    // effect system VPD sync.
                    throw std::runtime_error(
                        "Failed to check if backup & restore required. Error : " +
                        commonUtility::getErrCodeMsg(l_errCode));
                }

                // Implies backup and restore not required.
                return;
            }

            m_backupAndRestoreObj =
                std::make_shared<BackupAndRestore>(m_sysCfgJsonObj);
        }
    }
    catch (const std::exception& l_ex)
    {
        // PEL logged as system VPD sync will be effected without this
        // feature.
        types::PelInfoTuple l_pel(EventLogger::getErrorType(l_ex),
                                  types::SeverityType::Warning, 0, std::nullopt,
                                  std::nullopt, std::nullopt, std::nullopt);

        m_logger->logMessage(
            std::string("Back up and restore instantiation failed.") +
                EventLogger::getErrorMsg(l_ex),
            PlaceHolder::PEL, &l_pel);
    }
}

void IbmHandler::initEventListeners() noexcept
{
    try
    {
        m_eventListener =
            std::make_shared<Listener>(m_worker, m_asioConnection);
        m_eventListener->registerAssetTagChangeCallback();
        m_eventListener->registerHostStateChangeCallback();
        m_eventListener->registerPresenceChangeCallback();
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage("Failed to initialize event listener. Error: " +
                             std::string(l_ex.what()));
    }
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

            m_logger->logMessage("m_worker->isSystemVPDOnDBus() completed");

            m_progressInterface->set_property(
                "Status", std::string(constants::vpdCollectionCompleted));

            if (m_backupAndRestoreObj)
            {
                m_backupAndRestoreObj->backupAndRestore();
            }

            if (m_eventListener)
            {
                // Check if system config JSON specifies
                // correlatedPropertiesJson
                if (m_sysCfgJsonObj.contains("correlatedPropertiesConfigPath"))
                {
                    // register correlated properties callback with specific
                    // correlated properties JSON
                    m_eventListener->registerCorrPropCallBack(
                        m_sysCfgJsonObj["correlatedPropertiesConfigPath"]);
                }
                else
                {
                    m_logger->logMessage(
                        "Correlated properties JSON path is not defined in system config JSON. Correlated properties listener is disabled.");
                }
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
                logging::logMessage("Taking too long. Active thread = " +
                                    std::to_string(l_threadCount));
#ifdef ENABLE_FILE_LOGGING
                // terminate collection logger
                m_logger->terminateVpdCollectionLogging();
#endif
            }
            else
            {
                l_timerRetry++;
                logging::logMessage("Collection is in progress for [" +
                                    std::to_string(l_threadCount) + "] FRUs.");

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
                logging::logMessage(
                    "Failed to get inventory object path from JSON for FRU [" +
                    l_fruPath +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }

            o_failedPathList.push_back(l_fruPath);
            continue;
        }

        // check if the FRU is present
        if (!dbusUtility::isInventoryPresent(l_inventoryPath))
        {
            logging::logMessage(
                "Inventory not present, skip updating part number. Path: " +
                l_inventoryPath);
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
                        commonUtility::getErrCodeMsg(l_errCode));
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
                // Call dbus method to update on dbus
                if (!dbusUtility::publishVpdOnDBus(types::ObjectMap{
                        {l_inventoryPath,
                         {{constants::assetInf,
                           {{"SparePartNumber",
                             std::string(l_binaryKwdValue.begin(),
                                         l_binaryKwdValue.end())}}}}}}))
                {
                    logging::logMessage(
                        "Updating Spare Part Number under Asset interface failed for path [" +
                        l_inventoryPath + "]");
                }

                // Just needed for logging.
                std::string l_initialPartNum((*l_ptrToFn).begin(),
                                             (*l_ptrToFn).end());
                std::string l_finalPartNum(l_binaryKwdValue.begin(),
                                           l_binaryKwdValue.end());
                logging::logMessage(
                    "FRU Part number updated for path [" + l_inventoryPath +
                    "]" + "From [" + l_initialPartNum + "]" + " to [" +
                    l_finalPartNum + "]");
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
                logging::logMessage(
                    "Failed to check if the system is powerVs Configuration, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
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
        uint16_t l_errCode = 0;
        if (item.contains("holdidlepath"))
        {
            std::string cmd = "echo 0 > ";
            cmd += item["holdidlepath"];

            logging::logMessage("Enabling mux with command = " + cmd);

            commonUtility::executeCmd(cmd, l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to execute command [" + cmd +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }

            continue;
        }

        logging::logMessage(
            "Mux Entry does not have hold idle path. Can't enable the mux");
    }
}

void IbmHandler::getSystemJson(std::string& o_systemJson,
                               const types::VPDMapVariant& i_parsedVpdMap)
{
    if (auto l_pVal = std::get_if<types::IPZVpdMap>(&i_parsedVpdMap))
    {
        uint16_t l_errCode = 0;
        std::string l_hwKWdValue =
            vpdSpecificUtility::getHWVersion(*l_pVal, l_errCode);
        if (l_hwKWdValue.empty())
        {
            if (l_errCode)
            {
                throw DataException("Failed to fetch HW value. Reason: " +
                                    commonUtility::getErrCodeMsg(l_errCode));
            }
            throw DataException("HW value fetched is empty.");
        }

        const std::string& l_imKwdValue =
            vpdSpecificUtility::getIMValue(*l_pVal, l_errCode);
        if (l_imKwdValue.empty())
        {
            if (l_errCode)
            {
                throw DataException("Failed to fetch IM value. Reason: " +
                                    commonUtility::getErrCodeMsg(l_errCode));
            }
            throw DataException("IM value fetched is empty.");
        }

        auto l_itrToIM = config::systemType.find(l_imKwdValue);
        if (l_itrToIM == config::systemType.end())
        {
            throw DataException("IM keyword does not map to any system type");
        }

        const types::HWVerList l_hwVersionList = l_itrToIM->second.second;
        if (!l_hwVersionList.empty())
        {
            transform(l_hwKWdValue.begin(), l_hwKWdValue.end(),
                      l_hwKWdValue.begin(), ::toupper);

            auto l_itrToHW =
                std::find_if(l_hwVersionList.begin(), l_hwVersionList.end(),
                             [&l_hwKWdValue](const auto& l_aPair) {
                                 return l_aPair.first == l_hwKWdValue;
                             });

            if (l_itrToHW != l_hwVersionList.end())
            {
                if (!(*l_itrToHW).second.empty())
                {
                    o_systemJson += (*l_itrToIM).first + "_" +
                                    (*l_itrToHW).second + ".json";
                }
                else
                {
                    o_systemJson += (*l_itrToIM).first + ".json";
                }
                return;
            }
        }
        o_systemJson += l_itrToIM->second.first + ".json";
        return;
    }

    throw DataException(
        "Invalid VPD type returned from Parser. Can't get system JSON.");
}

void IbmHandler::setEnvAndReboot(const std::string& i_key,
                                 const std::string& i_value)
{
    // set env and reboot and break.
    uint16_t l_errCode = 0;
    commonUtility::executeCmd("/sbin/fw_setenv", l_errCode, i_key, i_value);

    if (l_errCode)
    {
        throw std::runtime_error(
            "Failed to execute command [/sbin/fw_setenv " + i_key + " " +
            i_value + "], error : " + commonUtility::getErrCodeMsg(l_errCode));
    }

#ifdef SKIP_REBOOT_ON_FITCONFIG_CHANGE
    m_logger->logMessage("NOT Rebooting BMC to pick up new device tree");
#else
    m_logger->logMessage("Rebooting BMC to pick up new device tree");

    // make dbus call to reboot
    auto l_bus = sdbusplus::bus::new_default_system();
    auto l_method = l_bus.new_method_call(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "Reboot");
    l_bus.call_noreply(l_method);
    exit(EXIT_SUCCESS);
#endif
}

std::string IbmHandler::readFitConfigValue()
{
    uint16_t l_errCode = 0;
    std::vector<std::string> l_output =
        commonUtility::executeCmd("/sbin/fw_printenv", l_errCode);

    if (l_errCode)
    {
        m_logger->logMessage(
            "Failed to execute command [/sbin/fw_printenv], error : " +
            commonUtility::getErrCodeMsg(l_errCode));
    }

    std::string l_fitConfigValue;

    for (const auto& l_entry : l_output)
    {
        auto l_pos = l_entry.find("=");
        auto l_key = l_entry.substr(0, l_pos);
        if (l_key != "fitconfig")
        {
            continue;
        }

        if (l_pos + 1 < l_entry.size())
        {
            l_fitConfigValue = l_entry.substr(l_pos + 1);
        }
    }

    return l_fitConfigValue;
}

bool IbmHandler::isBackupOnCache()
{
    try
    {
        uint16_t l_errCode = 0;
        std::string l_backupAndRestoreCfgFilePath =
            m_sysCfgJsonObj.value("backupRestoreConfigPath", "");

        if (l_backupAndRestoreCfgFilePath.empty())
        {
            m_logger->logMessage(
                "backupRestoreConfigPath is not found in JSON. Can't determne the backup path.");
            return false;
        }

        nlohmann::json l_backupAndRestoreCfgJsonObj =
            jsonUtility::getParsedJson(l_backupAndRestoreCfgFilePath,
                                       l_errCode);
        if (l_backupAndRestoreCfgJsonObj.empty() || l_errCode)
        {
            m_logger->logMessage(
                "JSON parsing failed for file [ " +
                std::string(l_backupAndRestoreCfgFilePath) +
                " ], error : " + commonUtility::getErrCodeMsg(l_errCode));
            return false;
        }

        // check if either of "source" or "destination" has inventory path.
        // this indicates that this sytem has System VPD on hardware
        // and other copy on D-Bus (BMC cache).
        if (!l_backupAndRestoreCfgJsonObj.empty() &&
            ((l_backupAndRestoreCfgJsonObj.contains("source") &&
              l_backupAndRestoreCfgJsonObj["source"].contains(
                  "inventoryPath")) ||
             (l_backupAndRestoreCfgJsonObj.contains("destination") &&
              l_backupAndRestoreCfgJsonObj["destination"].contains(
                  "inventoryPath"))))
        {
            return true;
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Exception while checking for backup on cache. Reason:" +
            std::string(l_ex.what()));
    }

    // In case of any failure/ambiguity. Don't perform back up and restore.
    return false;
}

void IbmHandler::performBackupAndRestore(types::VPDMapVariant& io_srcVpdMap)
{
    try
    {
        m_backupAndRestoreObj =
            std::make_shared<BackupAndRestore>(m_sysCfgJsonObj);
        auto [l_srcVpdVariant,
              l_dstVpdVariant] = m_backupAndRestoreObj->backupAndRestore();

        // ToDo: Revisit is this check is required or not.
        if (auto l_srcVpdMap = std::get_if<types::IPZVpdMap>(&l_srcVpdVariant);
            l_srcVpdMap && !(*l_srcVpdMap).empty())
        {
            io_srcVpdMap = std::move(l_srcVpdVariant);
        }
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Warning,
            __FILE__, __FUNCTION__, 0,
            std::string(
                "Exception caught while backup and restore VPD keyword's.") +
                EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

std::string IbmHandler::createAssetTagString(
    const types::VPDMapVariant& i_parsedVpdMap)
{
    std::string l_assetTag;
    // system VPD will be in IPZ format.
    if (auto l_parsedVpdMap = std::get_if<types::IPZVpdMap>(&i_parsedVpdMap))
    {
        auto l_itrToVsys = (*l_parsedVpdMap).find(constants::recVSYS);
        if (l_itrToVsys != (*l_parsedVpdMap).end())
        {
            uint16_t l_errCode = 0;
            const std::string l_tmKwdValue{vpdSpecificUtility::getKwVal(
                l_itrToVsys->second, constants::kwdTM, l_errCode)};
            if (l_tmKwdValue.empty())
            {
                throw std::runtime_error(
                    std::string("Failed to get value for keyword [") +
                    constants::kwdTM +
                    std::string("] while creating Asset tag. Error : " +
                                commonUtility::getErrCodeMsg(l_errCode)));
            }
            const std::string l_seKwdValue{vpdSpecificUtility::getKwVal(
                l_itrToVsys->second, constants::kwdSE, l_errCode)};
            if (l_seKwdValue.empty())
            {
                throw std::runtime_error(
                    std::string("Failed to get value for keyword [") +
                    constants::kwdSE +
                    std::string("] while creating Asset tag. Error : " +
                                commonUtility::getErrCodeMsg(l_errCode)));
            }
            l_assetTag = std::string{"Server-"} + l_tmKwdValue +
                         std::string{"-"} + l_seKwdValue;
        }
        else
        {
            throw std::runtime_error(
                "VSYS record not found in parsed VPD map to create Asset tag.");
        }
    }
    else
    {
        throw std::runtime_error(
            "Invalid VPD type recieved to create Asset tag.");
    }
    return l_assetTag;
}

void IbmHandler::publishSystemVPD(const types::VPDMapVariant& i_parsedVpdMap)
{
    types::ObjectMap l_objectInterfaceMap;
    if (std::get_if<types::IPZVpdMap>(&i_parsedVpdMap))
    {
        m_worker->populateDbus(i_parsedVpdMap, l_objectInterfaceMap,
                               SYSTEM_VPD_FILE_PATH);
        try
        {
            if (m_isFactoryResetDone)
            {
                const auto& l_assetTag = createAssetTagString(i_parsedVpdMap);
                auto l_itrToSystemPath = l_objectInterfaceMap.find(
                    sdbusplus::message::object_path(constants::systemInvPath));
                if (l_itrToSystemPath == l_objectInterfaceMap.end())
                {
                    throw std::runtime_error(
                        "Asset tag update failed. System Path not found in object map.");
                }
                types::PropertyMap l_assetTagProperty;
                l_assetTagProperty.emplace("AssetTag", l_assetTag);
                (l_itrToSystemPath->second)
                    .emplace(constants::assetTagInf,
                             std::move(l_assetTagProperty));
            }
        }
        catch (const std::exception& l_ex)
        {
            EventLogger::createSyncPel(
                EventLogger::getErrorType(l_ex), types::SeverityType::Warning,
                __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        }

        // Call method to update the dbus
        if (!dbusUtility::publishVpdOnDBus(move(l_objectInterfaceMap)))
        {
            throw std::runtime_error("Call to PIM failed for system VPD");
        }
    }
    else
    {
        throw DataException("Invalid format of parsed VPD map.");
    }
}

void IbmHandler::setJsonSymbolicLink(const std::string& i_systemJson)
{
    std::error_code l_ec;
    l_ec.clear();

    // Check if symlink file path exists and if the JSON at this location is a
    // symlink.
    if (m_isSymlinkPresent &&
        std::filesystem::is_symlink(INVENTORY_JSON_SYM_LINK, l_ec))
    {
        // Don't care about exception in "is_symlink". Will continue with
        // creationof symlink.
        const auto& l_symlinkFilePth =
            std::filesystem::read_symlink(INVENTORY_JSON_SYM_LINK, l_ec);
        if (l_ec)
        {
            logging::logMessage(
                "Can't read existing symlink. Error =" + l_ec.message() +
                "Trying removal of symlink and creation of new symlink.");
        }

        // If currently set JSON is the required one. No further processing
        // required.
        if (i_systemJson == l_symlinkFilePth)
        {
            // Correct symlink is already set.
            return;
        }

        if (!std::filesystem::remove(INVENTORY_JSON_SYM_LINK, l_ec))
        {
            // No point going further. If removal fails for existing symlink,
            // create will anyways throw.
            throw std::runtime_error(
                "Removal of symlink failed with Error = " + l_ec.message() +
                ". Can't proceed with create_symlink.");
        }
    }

    if (!std::filesystem::exists(VPD_SYMLIMK_PATH, l_ec))
    {
        if (l_ec)
        {
            throw std::runtime_error(
                "File system call to exist failed with error = " +
                l_ec.message());
        }

        // implies it is a fresh boot/factory reset.
        // Create the directory for hosting the symlink
        if (!std::filesystem::create_directories(VPD_SYMLIMK_PATH, l_ec))
        {
            if (l_ec)
            {
                throw std::runtime_error(
                    "File system call to create directory failed with error = " +
                    l_ec.message());
            }
        }
    }

    // create a new symlink based on the system
    std::filesystem::create_symlink(i_systemJson, INVENTORY_JSON_SYM_LINK,
                                    l_ec);
    if (l_ec)
    {
        throw std::runtime_error(
            "create_symlink system call failed with error: " + l_ec.message());
    }

    // update path to symlink.
    m_configJsonPath = INVENTORY_JSON_SYM_LINK;
    m_isSymlinkPresent = true;

    // If the flow is at this point implies the symlink was not present there.
    // Considering this as factory reset.
    m_isFactoryResetDone = true;
}

void IbmHandler::setDeviceTreeAndJson(
    types::VPDMapVariant& o_parsedSystemVpdMap)
{
    // JSON is madatory for processing of this API.
    if (m_sysCfgJsonObj.empty())
    {
        throw JsonException("System config JSON is empty", m_sysCfgJsonObj);
    }

    uint16_t l_errCode = 0;
    std::string l_systemVpdPath{SYSTEM_VPD_FILE_PATH};
    commonUtility::getEffectiveFruPath(m_vpdCollectionMode, l_systemVpdPath,
                                       l_errCode);

    if (l_errCode)
    {
        throw std::runtime_error(
            "Failed to get effective System VPD path, for [" + l_systemVpdPath +
            "], reason: " + commonUtility::getErrCodeMsg(l_errCode));
    }

    std::error_code l_ec;
    l_ec.clear();
    if (!std::filesystem::exists(l_systemVpdPath, l_ec))
    {
        std::string l_errMsg = "Can't Find System VPD file/eeprom. ";
        if (l_ec)
        {
            l_errMsg += l_ec.message();
        }

        // No point continuing without system VPD file
        throw std::runtime_error(l_errMsg);
    }

    // parse system VPD
    std::shared_ptr<Parser> l_vpdParser =
        std::make_shared<Parser>(l_systemVpdPath, m_sysCfgJsonObj);
    o_parsedSystemVpdMap = l_vpdParser->parse();

    if (std::holds_alternative<std::monostate>(o_parsedSystemVpdMap))
    {
        throw std::runtime_error(
            "System VPD parsing failed, from path [" + l_systemVpdPath +
            "]. Either file doesn't exist or error occurred while parsing the file.");
    }

    // Implies it is default JSON.
    std::string l_systemJson{JSON_ABSOLUTE_PATH_PREFIX};

    // get system JSON as per the system configuration.
    getSystemJson(l_systemJson, o_parsedSystemVpdMap);

    if (!l_systemJson.compare(JSON_ABSOLUTE_PATH_PREFIX))
    {
        throw DataException(
            "No system JSON found corresponding to IM read from VPD.");
    }

    // re-parse the JSON once appropriate JSON has been selected.
    m_sysCfgJsonObj = jsonUtility::getParsedJson(l_systemJson, l_errCode);

    if (l_errCode)
    {
        throw(JsonException(
            "JSON parsing failed for file [ " + l_systemJson +
                " ], error : " + commonUtility::getErrCodeMsg(l_errCode),
            l_systemJson));
    }

    vpdSpecificUtility::setCollectionStatusProperty(
        SYSTEM_VPD_FILE_PATH, types::VpdCollectionStatus::InProgress,
        m_sysCfgJsonObj, l_errCode);

    if (l_errCode)
    {
        m_logger->logMessage("Failed to set collection status for path " +
                             std::string(SYSTEM_VPD_FILE_PATH) + "Reason: " +
                             commonUtility::getErrCodeMsg(l_errCode));
    }

    std::string l_devTreeFromJson;
    if (m_sysCfgJsonObj.contains("devTree"))
    {
        l_devTreeFromJson = m_sysCfgJsonObj["devTree"];

        if (l_devTreeFromJson.empty())
        {
            EventLogger::createSyncPel(
                types::ErrorType::JsonFailure, types::SeverityType::Error,
                __FILE__, __FUNCTION__, 0,
                "Mandatory value for device tree missing from JSON[" +
                    l_systemJson + "]",
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        }
    }

    auto l_fitConfigVal = readFitConfigValue();

    if (l_devTreeFromJson.empty() ||
        l_fitConfigVal.find(l_devTreeFromJson) != std::string::npos)
    { // Skipping setting device tree as either devtree info is missing from
        // Json or it is rightly set.

        setJsonSymbolicLink(l_systemJson);

        const std::string& l_systemVpdInvPath =
            jsonUtility::getInventoryObjPathFromJson(
                m_sysCfgJsonObj, SYSTEM_VPD_FILE_PATH, l_errCode);

        if (l_systemVpdInvPath.empty())
        {
            if (l_errCode)
            {
                throw JsonException(
                    "System vpd inventory path not found in JSON. Reason:" +
                        commonUtility::getErrCodeMsg(l_errCode),
                    INVENTORY_JSON_SYM_LINK);
            }
            throw JsonException("System vpd inventory path is missing in JSON",
                                INVENTORY_JSON_SYM_LINK);
        }

        // TODO: for backward compatibility this should also support motherboard
        // interface.
        std::vector<std::string> l_interfaceList{
            constants::motherboardInterface};
        const types::MapperGetObject& l_sysVpdObjMap =
            dbusUtility::getObjectMap(l_systemVpdInvPath, l_interfaceList);

        if (!l_sysVpdObjMap.empty())
        {
            if (isBackupOnCache() && jsonUtility::isBackupAndRestoreRequired(
                                         m_sysCfgJsonObj, l_errCode))
            {
                performBackupAndRestore(o_parsedSystemVpdMap);
            }
            else if (l_errCode)
            {
                logging::logMessage(
                    "Failed to check if backup and restore required. Reason : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }
        }
        return;
    }

    setEnvAndReboot("fitconfig", l_devTreeFromJson);
#ifdef SKIP_REBOOT_ON_FITCONFIG_CHANGE
    setJsonSymbolicLink(l_systemJson);
#endif
}

void IbmHandler::performInitialSetup()
{
    // Parse whatever JSON is set as of now.
    uint16_t l_errCode = 0;
    try
    {
        m_sysCfgJsonObj =
            jsonUtility::getParsedJson(m_configJsonPath, l_errCode);

        if (l_errCode)
        {
            // Throwing as there is no point proceeding without any JSON.
            throw JsonException("JSON parsing failed. error : " +
                                    commonUtility::getErrCodeMsg(l_errCode),
                                m_configJsonPath);
        }

        types::VPDMapVariant l_parsedSysVpdMap;
        setDeviceTreeAndJson(l_parsedSysVpdMap);

        // now that correct JSON is selected, initialize worker class.
        initWorker();

        // proceed to publish system VPD.
        publishSystemVPD(l_parsedSysVpdMap);

        vpdSpecificUtility::setCollectionStatusProperty(
            SYSTEM_VPD_FILE_PATH, types::VpdCollectionStatus::Completed,
            m_sysCfgJsonObj, l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status for path " +
                std::string(SYSTEM_VPD_FILE_PATH) +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        // Set appropriate position of BMC.
        setBmcPosition();

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
        // Seeting of collection status should be utility method
        vpdSpecificUtility::setCollectionStatusProperty(
            SYSTEM_VPD_FILE_PATH, types::VpdCollectionStatus::Failed,
            m_sysCfgJsonObj, l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to set collection status for path " +
                std::string(SYSTEM_VPD_FILE_PATH) +
                "Reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        // Any issue in system's inital set up is handled in this catch. Error
        // will not propogate to manager.
        const types::PelInfoTuple l_pel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Critical, 0,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);

        m_logger->logMessage(
            std::string("Exception while performing initial set up. ") +
                EventLogger::getErrorMsg(l_ex),
            PlaceHolder::PEL, &l_pel);
    }
}

void IbmHandler::setBmcPosition()
{
    size_t l_bmcPosition = dbusUtility::getBmcPosition();

    // Workaround until getBmcPosition() is filled in and
    // doesn't just return max().
    if (l_bmcPosition == std::numeric_limits<size_t>::max())
    {
        l_bmcPosition = 0;
    }

    uint16_t l_errCode = 0;
    // Special Handling required for RBMC prototype system as Cable Management
    // Daemon is not there.
    if (isRbmcPrototypeSystem(l_errCode))
    {
        checkAndUpdateBmcPosition(l_bmcPosition);
    }
    else if (l_errCode != 0)
    {
        m_logger->logMessage(
            "Unable to determine whether system is RBMC system or not, reason: " +
            commonUtility::getErrCodeMsg(l_errCode));
    }

    // Call method to update the dbus
    if (!dbusUtility::publishVpdOnDBus(types::ObjectMap{
            {sdbusplus::message::object_path(constants::systemInvPath),
             {{constants::rbmcPositionInterface,
               {{"Position", l_bmcPosition}}}}}}))
    {
        m_logger->logMessage(
            "Updating BMC position failed for path [" +
            std::string(constants::systemInvPath) +
            "], bmc position: " + std::to_string(l_bmcPosition));

        // ToDo: Check if PEL required
    }

    writeBmcPositionToFile(l_bmcPosition);
}

void IbmHandler::writeBmcPositionToFile(const size_t i_bmcPosition)
{
    const std::filesystem::path l_filePath{constants::bmcPositionFile};

    std::error_code l_ec;
    if (!std::filesystem::exists(l_filePath.parent_path(), l_ec))
    {
        std::filesystem::create_directories(l_filePath.parent_path(), l_ec);
        if (l_ec)
        {
            m_logger->logMessage("create_directories() failed on " +
                                 l_filePath.parent_path().string() +
                                 ". Error =" + l_ec.message());
            return;
        }
    }

    try
    {
        std::ofstream l_outFile;
        l_outFile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        l_outFile.open(l_filePath);
        if (!l_outFile)
        {
            m_logger->logMessage("Failed to open file [" + l_filePath.string() +
                                 "] for writing");
            return;
        }

        l_outFile << i_bmcPosition;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::string("Exception while writing BMC position to file: ") +
            l_ex.what());
    }
}

void IbmHandler::collectAllFruVpd()
{
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
    if (m_sysCfgJsonObj.empty())
    {
        m_logger->logMessage(
            "System config JSON is empty, unable to find BMC position");
        return;
    }

    uint16_t l_errCode = 0;
    std::string l_motherboardEepromPath = jsonUtility::getFruPathFromJson(
        m_sysCfgJsonObj, constants::systemVpdInvPath, l_errCode);

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
