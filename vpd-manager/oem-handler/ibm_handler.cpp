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
    performInitialSetup();

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

    // Instantiate Listener object
    m_eventListener = std::make_shared<Listener>(m_worker, m_asioConnection);
    m_eventListener->registerAssetTagChangeCallback();
    m_eventListener->registerHostStateChangeCallback();
    m_eventListener->registerPresenceChangeCallback();

    // set async timer to detect if system VPD is published on D-Bus.
    SetTimerToDetectSVPDOnDbus();

    // set async timer to detect if VPD collection is done.
    SetTimerToDetectVpdCollectionStatus();

    // Instantiate GpioMonitor class
    m_gpioMonitor =
        std::make_shared<GpioMonitor>(m_sysCfgJsonObj, m_worker, m_ioContext);
}

void IbmHandler::SetTimerToDetectSVPDOnDbus()
{
    try
    {
        static boost::asio::steady_timer timer(*m_ioContext);

        // timer for 2 seconds
        auto asyncCancelled = timer.expires_after(std::chrono::seconds(2));

        (asyncCancelled == 0) ? logging::logMessage("Timer started")
                              : logging::logMessage("Timer re-started");

        timer.async_wait([this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
            {
                throw std::runtime_error(
                    std::string(__FUNCTION__) +
                    ": Timer to detect system VPD collection status was aborted.");
            }

            if (ec)
            {
                throw std::runtime_error(
                    std::string(__FUNCTION__) +
                    ": Timer to detect System VPD collection failed");
            }

            if (m_worker->isSystemVPDOnDBus())
            {
                // cancel the timer
                timer.cancel();

                // Triggering FRU VPD collection. Setting status to "In
                // Progress".
                m_interface->set_property("CollectionStatus",
                                          std::string("InProgress"));
                m_worker->collectFrusFromJson();
            }
        });
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createAsyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Critical,
            __FILE__, __FUNCTION__, 0,
            std::string("Collection for FRUs failed with reason:") +
                EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
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

            std::cout << "m_worker->isSystemVPDOnDBus() completed" << std::endl;
            m_interface->set_property("CollectionStatus",
                                      std::string("Completed"));

            if (m_backupAndRestoreObj)
            {
                m_backupAndRestoreObj->backupAndRestore();
            }

            if (m_eventListener)
            {
                m_eventListener->registerCorrPropCallBack();
            }
        }
        else
        {
            auto l_threadCount = m_worker->getActiveThreadCount();
            if (l_timerRetry == MAX_RETRY)
            {
                l_timer.cancel();
                logging::logMessage("Taking too long. Active thread = " +
                                    std::to_string(l_threadCount));
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
        auto l_inventoryPath = jsonUtility::getInventoryObjPathFromJson(
            l_sysCfgJsonObj, l_fruPath);

        // Mark it as failed if inventory path not found in JSON.
        if (l_inventoryPath.empty())
        {
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
                vpdSpecificUtility::getCcinFromDbus(l_inventoryPath);

            // Not an ideal situation as CCIN can't be empty.
            if (l_ccinFromDbus.empty())
            {
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

        if (!vpdSpecificUtility::isPowerVsConfiguration(l_imValue))
        {
            // TODO: Should booting be blocked in case of some
            // misconfigurations?
            return;
        }

        const nlohmann::json& l_powerVsJsonObj =
            jsonUtility::getPowerVsJson(l_imValue);

        if (l_powerVsJsonObj.empty())
        {
            throw std::runtime_error("PowerVS Json not found");
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

void IbmHandler::primeSystemBlueprint()
{
    if (m_sysCfgJsonObj.empty())
    {
        return;
    }

    const nlohmann::json& l_listOfFrus =
        m_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& l_itemFRUS : l_listOfFrus.items())
    {
        const std::string& l_vpdFilePath = l_itemFRUS.key();

        if (l_vpdFilePath == SYSTEM_VPD_FILE_PATH)
        {
            continue;
        }

        // Prime the inventry for FRUs which
        // are not present/processing had some error.
        if (m_worker.get() != nullptr &&
            !m_worker->primeInventory(l_vpdFilePath))
        {
            logging::logMessage(
                "Priming of inventory failed for FRU " + l_vpdFilePath);
        }
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

            if (isPrimingRequired())
            {
                primeSystemBlueprint();
            }
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
        // Any issue in system's inital set up is handled in this catch. Error
        // will not propogate to manager.
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Critical,
            __FILE__, __FUNCTION__, 0, EventLogger::getErrorMsg(l_ex),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

bool IbmHandler::isPrimingRequired() const noexcept
{
    try
    {
        // get all object paths under PIM
        const auto l_objectPaths = dbusUtility::GetSubTreePaths(
            constants::systemInvPath, 0,
            std::vector<std::string>{constants::vpdCollectionInterface});

        const nlohmann::json& l_listOfFrus =
            m_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

        size_t l_invPathCount = 0;

        for (const auto& l_itemFRUS : l_listOfFrus.items())
        {
            for (const auto& l_Fru : l_itemFRUS.value())
            {
                if (l_Fru.contains("ccin") || (l_Fru.contains("noprime") &&
                                               l_Fru.value("noprime", false)))
                {
                    continue;
                }

                l_invPathCount += 1;
            }
        }
        return ((l_objectPaths.size() >= l_invPathCount) ? false : true);
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Error while checking is priming required or not, error: " +
            std::string(l_ex.what()));
    }

    // In case of any error, perform priming, as it's unclear whether priming is
    // required.
    return true;
}

void IbmHandler::collectAllFruVpd()
{
    // Setting status to "InProgress", before trigeering VPD collection.
    m_interface->set_property("CollectionStatus", std::string("InProgress"));
    m_worker->collectFrusFromJson();
    SetTimerToDetectVpdCollectionStatus();
}
} // namespace vpd
