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
    registerAssetTagChangeCallback();

    // set async timer to detect if system VPD is published on D-Bus.
    SetTimerToDetectSVPDOnDbus();

    // set async timer to detect if VPD collection is done.
    SetTimerToDetectVpdCollectionStatus();

    // Instantiate GpioMonitor class
    m_gpioMonitor =
        std::make_shared<GpioMonitor>(m_sysCfgJsonObj, m_worker, m_ioContext);
}

void IbmManager::registerAssetTagChangeCallback()
{
    static std::shared_ptr<sdbusplus::bus::match_t> l_assetMatch =
        std::make_shared<sdbusplus::bus::match_t>(
            *m_asioConnection,
            sdbusplus::bus::match::rules::propertiesChanged(
                constants::systemInvPath, constants::assetTagInf),
            [this](sdbusplus::message_t& l_msg) {
                processAssetTagChangeCallback(l_msg);
            });
}

void IbmManager::processAssetTagChangeCallback(sdbusplus::message_t& i_msg)
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw std::runtime_error(
                "Error reading callback msg for asset tag.");
        }

        std::string l_objectPath;
        types::PropertyMap l_propMap;
        i_msg.read(l_objectPath, l_propMap);

        const auto& l_itrToAssetTag = l_propMap.find("AssetTag");
        if (l_itrToAssetTag != l_propMap.end())
        {
            if (auto l_assetTag =
                    std::get_if<std::string>(&(l_itrToAssetTag->second)))
            {
                // Call Notify to persist the AssetTag
                types::ObjectMap l_objectMap = {
                    {sdbusplus::message::object_path(constants::systemInvPath),
                     {{constants::assetTagInf, {{"AssetTag", *l_assetTag}}}}}};

                // Notify PIM
                if (!dbusUtility::callPIM(move(l_objectMap)))
                {
                    throw std::runtime_error(
                        "Call to PIM failed for asset tag update.");
                }
            }
        }
        else
        {
            throw std::runtime_error(
                "Could not find asset tag in callback message.");
        }
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Log PEL with below description.
        logging::logMessage("Asset tag callback update failed with error: " +
                            std::string(l_ex.what()));
    }
}

void IbmManager::SetTimerToDetectSVPDOnDbus()
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

void IbmManager::SetTimerToDetectVpdCollectionStatus()
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

            m_interface->set_property("CollectionStatus",
                                      std::string("Completed"));

            if (m_backupAndRestoreObj)
            {
                m_backupAndRestoreObj->backupAndRestore();
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

void IbmManager::checkAndUpdatePowerVsVpd(
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
                /*  if (updateKeyword(
                          l_fruPath,
                          std::make_tuple(l_recordName, l_kwdName, l_kwdValue))
                  == constants::FAILURE)
                  {
                      o_failedPathList.push_back(l_fruPath);
                      continue;
                  }*/
                // use this method instead.
                /*  std::shared_ptr<Parser> l_parserObj =
                  std::make_shared<Parser>(l_fruPath, l_sysCfgJsonObj);
                  auto l_rc =
                  l_parserObj->updateVpdKeyword(i_paramsToWriteData); */

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

void IbmManager::ConfigurePowerVsSystem()
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

void IbmManager::processFailedEeproms()
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
} // namespace vpd
