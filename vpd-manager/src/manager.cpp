#include "config.h"

#include "manager.hpp"

#include "backup_restore.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "parser.hpp"
#include "parser_factory.hpp"
#include "parser_interface.hpp"
#include "types.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/json_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

namespace vpd
{
Manager::Manager(
    const std::shared_ptr<boost::asio::io_context>& ioCon,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& iFace,
    const std::shared_ptr<sdbusplus::asio::connection>& asioConnection) :
    m_ioContext(ioCon), m_interface(iFace), m_asioConnection(asioConnection)
{
    try
    {
#ifdef IBM_SYSTEM
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

        if (!m_worker->getSysCfgJsonObj().empty() &&
            jsonUtility::isBackupAndRestoreRequired(l_sysCfgJsonObj))
        {
            m_backupAndRestoreObj = std::make_shared<BackupAndRestore>(
                m_worker->getSysCfgJsonObj());
        }

        // set callback to detect any asset tag change
        registerAssetTagChangeCallback();

        // set async timer to detect if system VPD is published on D-Bus.
        SetTimerToDetectSVPDOnDbus();

        // set async timer to detect if VPD collection is done.
        SetTimerToDetectVpdCollectionStatus();

        // Instantiate GpioMonitor class
        m_gpioMonitor = std::make_shared<GpioMonitor>(
            m_worker->getSysCfgJsonObj(), m_worker, m_ioContext);

#endif
        // set callback to detect host state change.
        registerHostStateChangeCallback();

        // For backward compatibility. Should be depricated.
        iFace->register_method(
            "WriteKeyword",
            [this](const sdbusplus::message::object_path i_path,
                   const std::string i_recordName, const std::string i_keyword,
                   const types::BinaryVector i_value) -> int {
                return this->updateKeyword(
                    i_path, std::make_tuple(i_recordName, i_keyword, i_value));
            });

        // Register methods under com.ibm.VPD.Manager interface
        iFace->register_method(
            "UpdateKeyword",
            [this](const types::Path i_vpdPath,
                   const types::WriteVpdParams i_paramsToWriteData) -> int {
                return this->updateKeyword(i_vpdPath, i_paramsToWriteData);
            });

        iFace->register_method(
            "WriteKeywordOnHardware",
            [this](const types::Path i_fruPath,
                   const types::WriteVpdParams i_paramsToWriteData) -> int {
                return this->updateKeywordOnHardware(i_fruPath,
                                                     i_paramsToWriteData);
            });

        iFace->register_method(
            "ReadKeyword",
            [this](const types::Path i_fruPath,
                   const types::ReadVpdParams i_paramsToReadData)
                -> types::DbusVariantType {
                return this->readKeyword(i_fruPath, i_paramsToReadData);
            });

        iFace->register_method(
            "CollectFRUVPD",
            [this](const sdbusplus::message::object_path& i_dbusObjPath) {
                this->collectSingleFruVpd(i_dbusObjPath);
            });

        iFace->register_method(
            "deleteFRUVPD",
            [this](const sdbusplus::message::object_path& i_dbusObjPath) {
                this->deleteSingleFruVpd(i_dbusObjPath);
            });

        iFace->register_method(
            "GetExpandedLocationCode",
            [this](const std::string& i_unexpandedLocationCode,
                   uint16_t& i_nodeNumber) -> std::string {
                return this->getExpandedLocationCode(i_unexpandedLocationCode,
                                                     i_nodeNumber);
            });

        iFace->register_method("GetFRUsByExpandedLocationCode",
                               [this](const std::string& i_expandedLocationCode)
                                   -> types::ListOfPaths {
                                   return this->getFrusByExpandedLocationCode(
                                       i_expandedLocationCode);
                               });

        iFace->register_method(
            "GetFRUsByUnexpandedLocationCode",
            [this](const std::string& i_unexpandedLocationCode,
                   uint16_t& i_nodeNumber) -> types::ListOfPaths {
                return this->getFrusByUnexpandedLocationCode(
                    i_unexpandedLocationCode, i_nodeNumber);
            });

        iFace->register_method(
            "GetHardwarePath",
            [this](const sdbusplus::message::object_path& i_dbusObjPath)
                -> std::string { return this->getHwPath(i_dbusObjPath); });

        iFace->register_method("PerformVPDRecollection", [this]() {
            this->performVpdRecollection();
        });

        // Indicates FRU VPD collection for the system has not started.
        iFace->register_property_rw<std::string>(
            "CollectionStatus", sdbusplus::vtable::property_::emits_change,
            [this](const std::string l_currStatus, const auto&) {
                m_vpdCollectionStatus = l_currStatus;
                return 0;
            },
            [this](const auto&) { return m_vpdCollectionStatus; });
    }
    catch (const std::exception& e)
    {
        logging::logMessage(
            "VPD-Manager service failed. " + std::string(e.what()));
        throw;
    }
}

#ifdef IBM_SYSTEM
void Manager::registerAssetTagChangeCallback()
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

void Manager::processAssetTagChangeCallback(sdbusplus::message_t& i_msg)
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

void Manager::SetTimerToDetectSVPDOnDbus()
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

void Manager::SetTimerToDetectVpdCollectionStatus()
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

void Manager::checkAndUpdatePowerVsVpd(
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
                if (updateKeyword(
                        l_fruPath,
                        std::make_tuple(l_recordName, l_kwdName, l_kwdValue)) ==
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

void Manager::ConfigurePowerVsSystem()
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

void Manager::processFailedEeproms()
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
#endif

int Manager::updateKeyword(const types::Path i_vpdPath,
                           const types::WriteVpdParams i_paramsToWriteData)
{
    if (i_vpdPath.empty())
    {
        logging::logMessage("Given VPD path is empty.");
        return -1;
    }

    types::Path l_fruPath;
    nlohmann::json l_sysCfgJsonObj{};

    if (m_worker.get() != nullptr)
    {
        l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();

        // Get the EEPROM path
        if (!l_sysCfgJsonObj.empty())
        {
            l_fruPath =
                jsonUtility::getFruPathFromJson(l_sysCfgJsonObj, i_vpdPath);
        }
    }

    if (l_fruPath.empty())
    {
        l_fruPath = i_vpdPath;
    }

    try
    {
        std::shared_ptr<Parser> l_parserObj =
            std::make_shared<Parser>(l_fruPath, l_sysCfgJsonObj);

        int l_rc = l_parserObj->updateVpdKeyword(i_paramsToWriteData);

        if (l_rc > constants::VALUE_0 && m_backupRestoreObj)
        {
            if (m_backupRestoreObj->perfromIndividualBackupRestore(
                    l_fruPath, i_paramsToWriteData) < 0)
            {
                // write success but back up and restore failed;
            }
        }

        return l_rc;
    }
    catch (const std::exception& l_exception)
    {
        // TODO:: error log needed
        logging::logMessage("Update keyword failed for file[" + i_vpdPath +
                            "], reason: " + std::string(l_exception.what()));
        return -1;
    }
}

int Manager::updateKeywordOnHardware(
    const types::Path i_fruPath,
    const types::WriteVpdParams i_paramsToWriteData) noexcept
{
    try
    {
        if (i_fruPath.empty())
        {
            throw std::runtime_error("Given FRU path is empty");
        }

        nlohmann::json l_sysCfgJsonObj{};

        if (m_worker.get() != nullptr)
        {
            l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();
        }

        std::shared_ptr<Parser> l_parserObj =
            std::make_shared<Parser>(i_fruPath, l_sysCfgJsonObj);
        return l_parserObj->updateVpdKeywordOnHardware(i_paramsToWriteData);
    }
    catch (const std::exception& l_exception)
    {
        EventLogger::createAsyncPel(
            types::ErrorType::InvalidEeprom, types::SeverityType::Informational,
            __FILE__, __FUNCTION__, 0,
            "Update keyword on hardware failed for file[" + i_fruPath +
                "], reason: " + std::string(l_exception.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);

        return constants::FAILURE;
    }
}

types::DbusVariantType Manager::readKeyword(
    const types::Path i_fruPath, const types::ReadVpdParams i_paramsToReadData)
{
    try
    {
        nlohmann::json l_jsonObj{};

        if (m_worker.get() != nullptr)
        {
            l_jsonObj = m_worker->getSysCfgJsonObj();
        }

        std::error_code ec;

        // Check if given path is filesystem path
        if (!std::filesystem::exists(i_fruPath, ec) && (ec))
        {
            throw std::runtime_error(
                "Given file path " + i_fruPath + " not found.");
        }

        logging::logMessage("Performing VPD read on " + i_fruPath);

        std::shared_ptr<vpd::Parser> l_parserObj =
            std::make_shared<vpd::Parser>(i_fruPath, l_jsonObj);

        std::shared_ptr<vpd::ParserInterface> l_vpdParserInstance =
            l_parserObj->getVpdParserInstance();

        return (
            l_vpdParserInstance->readKeywordFromHardware(i_paramsToReadData));
    }
    catch (const std::exception& e)
    {
        logging::logMessage(
            e.what() + std::string(". VPD manager read operation failed for ") +
            i_fruPath);
        throw types::DeviceError::ReadFailure();
    }
}

void Manager::collectSingleFruVpd(
    const sdbusplus::message::object_path& i_dbusObjPath)
{
    try
    {
        if (m_vpdCollectionStatus != "Completed")
        {
            logging::logMessage(
                "Currently VPD CollectionStatus is not completed. Cannot perform single FRU VPD collection for " +
                std::string(i_dbusObjPath));
            return;
        }

        // Get system config JSON object from worker class
        nlohmann::json l_sysCfgJsonObj{};

        if (m_worker.get() != nullptr)
        {
            l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();
        }

        // Check if system config JSON is present
        if (l_sysCfgJsonObj.empty())
        {
            logging::logMessage(
                "System config JSON object not present. Single FRU VPD collection is not performed for " +
                std::string(i_dbusObjPath));
            return;
        }

        // Get FRU path for the given D-bus object path from JSON
        const std::string& l_fruPath =
            jsonUtility::getFruPathFromJson(l_sysCfgJsonObj, i_dbusObjPath);

        if (l_fruPath.empty())
        {
            logging::logMessage(
                "D-bus object path not present in JSON. Single FRU VPD collection is not performed for " +
                std::string(i_dbusObjPath));
            return;
        }

        // Check if host is up and running
        if (dbusUtility::isHostRunning())
        {
            if (!jsonUtility::isFruReplaceableAtRuntime(l_sysCfgJsonObj,
                                                        l_fruPath))
            {
                logging::logMessage(
                    "Given FRU is not replaceable at host runtime. Single FRU VPD collection is not performed for " +
                    std::string(i_dbusObjPath));
                return;
            }
        }
        else if (dbusUtility::isBMCReady())
        {
            if (!jsonUtility::isFruReplaceableAtStandby(l_sysCfgJsonObj,
                                                        l_fruPath) &&
                (!jsonUtility::isFruReplaceableAtRuntime(l_sysCfgJsonObj,
                                                         l_fruPath)))
            {
                logging::logMessage(
                    "Given FRU is neither replaceable at standby nor replaceable at runtime. Single FRU VPD collection is not performed for " +
                    std::string(i_dbusObjPath));
                return;
            }
        }

        // Set CollectionStatus as InProgress. Since it's an intermediate state
        // D-bus set-property call is good enough to update the status.
        const std::string& l_collStatusProp = "CollectionStatus";

        if (!dbusUtility::writeDbusProperty(
                jsonUtility::getServiceName(l_sysCfgJsonObj,
                                            std::string(i_dbusObjPath)),
                std::string(i_dbusObjPath), constants::vpdCollectionInterface,
                l_collStatusProp,
                types::DbusVariantType{constants::vpdCollectionInProgress}))
        {
            logging::logMessage(
                "Unable to set CollectionStatus as InProgress for " +
                std::string(i_dbusObjPath) +
                ". Continue single FRU VPD collection.");
        }

        // Parse VPD
        types::VPDMapVariant l_parsedVpd = m_worker->parseVpdFile(l_fruPath);

        // If l_parsedVpd is pointing to std::monostate
        if (l_parsedVpd.index() == 0)
        {
            throw std::runtime_error(
                "VPD parsing failed for " + std::string(i_dbusObjPath));
        }

        // Get D-bus object map from worker class
        types::ObjectMap l_dbusObjectMap;
        m_worker->populateDbus(l_parsedVpd, l_dbusObjectMap, l_fruPath);

        if (l_dbusObjectMap.empty())
        {
            throw std::runtime_error(
                "Failed to create D-bus object map. Single FRU VPD collection failed for " +
                std::string(i_dbusObjPath));
        }

        // Call PIM's Notify method
        if (!dbusUtility::callPIM(move(l_dbusObjectMap)))
        {
            throw std::runtime_error(
                "Notify PIM failed. Single FRU VPD collection failed for " +
                std::string(i_dbusObjPath));
        }
    }
    catch (const std::exception& l_error)
    {
        // Notify FRU's VPD CollectionStatus as Failure
        if (!dbusUtility::notifyFRUCollectionStatus(
                std::string(i_dbusObjPath), constants::vpdCollectionFailure))
        {
            logging::logMessage(
                "Call to PIM Notify method failed to update Collection status as Failure for " +
                std::string(i_dbusObjPath));
        }

        // TODO: Log PEL
        logging::logMessage(std::string(l_error.what()));
    }
}

void Manager::deleteSingleFruVpd(
    const sdbusplus::message::object_path& i_dbusObjPath)
{
    try
    {
        if (std::string(i_dbusObjPath).empty())
        {
            throw std::runtime_error(
                "Given DBus object path is empty. Aborting FRU VPD deletion.");
        }

        if (m_worker.get() == nullptr)
        {
            throw std::runtime_error(
                "Worker object not found, can't perform FRU VPD deletion for: " +
                std::string(i_dbusObjPath));
        }

        m_worker->deleteFruVpd(std::string(i_dbusObjPath));
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Log PEL
        logging::logMessage(l_ex.what());
    }
}

bool Manager::isValidUnexpandedLocationCode(
    const std::string& i_unexpandedLocationCode)
{
    if ((i_unexpandedLocationCode.length() <
         constants::UNEXP_LOCATION_CODE_MIN_LENGTH) ||
        ((i_unexpandedLocationCode.compare(0, 4, "Ufcs") !=
          constants::STR_CMP_SUCCESS) &&
         (i_unexpandedLocationCode.compare(0, 4, "Umts") !=
          constants::STR_CMP_SUCCESS)) ||
        ((i_unexpandedLocationCode.length() >
          constants::UNEXP_LOCATION_CODE_MIN_LENGTH) &&
         (i_unexpandedLocationCode.find("-") != 4)))
    {
        return false;
    }

    return true;
}

std::string Manager::getExpandedLocationCode(
    const std::string& i_unexpandedLocationCode,
    [[maybe_unused]] const uint16_t i_nodeNumber)
{
    if (!isValidUnexpandedLocationCode(i_unexpandedLocationCode))
    {
        phosphor::logging::elog<types::DbusInvalidArgument>(
            types::InvalidArgument::ARGUMENT_NAME("LOCATIONCODE"),
            types::InvalidArgument::ARGUMENT_VALUE(
                i_unexpandedLocationCode.c_str()));
    }

    const nlohmann::json& l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();
    if (!l_sysCfgJsonObj.contains("frus"))
    {
        logging::logMessage("Missing frus tag in system config JSON");
    }

    const nlohmann::json& l_listOfFrus =
        l_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& l_frus : l_listOfFrus.items())
    {
        for (const auto& l_aFru : l_frus.value())
        {
            if (l_aFru["extraInterfaces"].contains(
                    constants::locationCodeInf) &&
                l_aFru["extraInterfaces"][constants::locationCodeInf].value(
                    "LocationCode", "") == i_unexpandedLocationCode)
            {
                return std::get<std::string>(dbusUtility::readDbusProperty(
                    l_aFru["serviceName"], l_aFru["inventoryPath"],
                    constants::locationCodeInf, "LocationCode"));
            }
        }
    }
    phosphor::logging::elog<types::DbusInvalidArgument>(
        types::InvalidArgument::ARGUMENT_NAME("LOCATIONCODE"),
        types::InvalidArgument::ARGUMENT_VALUE(
            i_unexpandedLocationCode.c_str()));
}

types::ListOfPaths Manager::getFrusByUnexpandedLocationCode(
    const std::string& i_unexpandedLocationCode,
    [[maybe_unused]] const uint16_t i_nodeNumber)
{
    types::ListOfPaths l_inventoryPaths;

    if (!isValidUnexpandedLocationCode(i_unexpandedLocationCode))
    {
        phosphor::logging::elog<types::DbusInvalidArgument>(
            types::InvalidArgument::ARGUMENT_NAME("LOCATIONCODE"),
            types::InvalidArgument::ARGUMENT_VALUE(
                i_unexpandedLocationCode.c_str()));
    }

    const nlohmann::json& l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();
    if (!l_sysCfgJsonObj.contains("frus"))
    {
        logging::logMessage("Missing frus tag in system config JSON");
    }

    const nlohmann::json& l_listOfFrus =
        l_sysCfgJsonObj["frus"].get_ref<const nlohmann::json::object_t&>();

    for (const auto& l_frus : l_listOfFrus.items())
    {
        for (const auto& l_aFru : l_frus.value())
        {
            if (l_aFru["extraInterfaces"].contains(
                    constants::locationCodeInf) &&
                l_aFru["extraInterfaces"][constants::locationCodeInf].value(
                    "LocationCode", "") == i_unexpandedLocationCode)
            {
                l_inventoryPaths.push_back(
                    l_aFru.at("inventoryPath")
                        .get_ref<const nlohmann::json::string_t&>());
            }
        }
    }

    if (l_inventoryPaths.empty())
    {
        phosphor::logging::elog<types::DbusInvalidArgument>(
            types::InvalidArgument::ARGUMENT_NAME("LOCATIONCODE"),
            types::InvalidArgument::ARGUMENT_VALUE(
                i_unexpandedLocationCode.c_str()));
    }

    return l_inventoryPaths;
}

std::string Manager::getHwPath(
    const sdbusplus::message::object_path& i_dbusObjPath)
{
    // Dummy code to supress unused variable warning. To be removed.
    logging::logMessage(std::string(i_dbusObjPath));

    return std::string{};
}

std::tuple<std::string, uint16_t> Manager::getUnexpandedLocationCode(
    const std::string& i_expandedLocationCode)
{
    /**
     * Location code should always start with U and fulfil minimum length
     * criteria.
     */
    if (i_expandedLocationCode[0] != 'U' ||
        i_expandedLocationCode.length() <
            constants::EXP_LOCATION_CODE_MIN_LENGTH)
    {
        phosphor::logging::elog<types::DbusInvalidArgument>(
            types::InvalidArgument::ARGUMENT_NAME("LOCATIONCODE"),
            types::InvalidArgument::ARGUMENT_VALUE(
                i_expandedLocationCode.c_str()));
    }

    std::string l_fcKwd;

    auto l_fcKwdValue = dbusUtility::readDbusProperty(
        "xyz.openbmc_project.Inventory.Manager",
        "/xyz/openbmc_project/inventory/system/chassis/motherboard",
        "com.ibm.ipzvpd.VCEN", "FC");

    if (auto l_kwdValue = std::get_if<types::BinaryVector>(&l_fcKwdValue))
    {
        l_fcKwd.assign(l_kwdValue->begin(), l_kwdValue->end());
    }

    // Get the first part of expanded location code to check for FC or TM.
    std::string l_firstKwd = i_expandedLocationCode.substr(1, 4);

    std::string l_unexpandedLocationCode{};
    uint16_t l_nodeNummber = constants::INVALID_NODE_NUMBER;

    // Check if this value matches the value of FC keyword.
    if (l_fcKwd.substr(0, 4) == l_firstKwd)
    {
        /**
         * Period(.) should be there in expanded location code to seggregate
         * FC, node number and SE values.
         */
        size_t l_nodeStartPos = i_expandedLocationCode.find('.');
        if (l_nodeStartPos == std::string::npos)
        {
            phosphor::logging::elog<types::DbusInvalidArgument>(
                types::InvalidArgument::ARGUMENT_NAME("LOCATIONCODE"),
                types::InvalidArgument::ARGUMENT_VALUE(
                    i_expandedLocationCode.c_str()));
        }

        size_t l_nodeEndPos =
            i_expandedLocationCode.find('.', l_nodeStartPos + 1);
        if (l_nodeEndPos == std::string::npos)
        {
            phosphor::logging::elog<types::DbusInvalidArgument>(
                types::InvalidArgument::ARGUMENT_NAME("LOCATIONCODE"),
                types::InvalidArgument::ARGUMENT_VALUE(
                    i_expandedLocationCode.c_str()));
        }

        // Skip 3 bytes for '.ND'
        l_nodeNummber = std::stoi(i_expandedLocationCode.substr(
            l_nodeStartPos + 3, (l_nodeEndPos - l_nodeStartPos - 3)));

        /**
         * Confirm if there are other details apart FC, node number and SE
         * in location code
         */
        if (i_expandedLocationCode.length() >
            constants::EXP_LOCATION_CODE_MIN_LENGTH)
        {
            l_unexpandedLocationCode =
                i_expandedLocationCode[0] + std::string("fcs") +
                i_expandedLocationCode.substr(
                    l_nodeEndPos + 1 + constants::SE_KWD_LENGTH,
                    std::string::npos);
        }
        else
        {
            l_unexpandedLocationCode = "Ufcs";
        }
    }
    else
    {
        std::string l_tmKwd;
        // Read TM keyword value.
        auto l_tmKwdValue = dbusUtility::readDbusProperty(
            "xyz.openbmc_project.Inventory.Manager",
            "/xyz/openbmc_project/inventory/system/chassis/motherboard",
            "com.ibm.ipzvpd.VSYS", "TM");

        if (auto l_kwdValue = std::get_if<types::BinaryVector>(&l_tmKwdValue))
        {
            l_tmKwd.assign(l_kwdValue->begin(), l_kwdValue->end());
        }

        // Check if the substr matches to TM keyword value.
        if (l_tmKwd.substr(0, 4) == l_firstKwd)
        {
            /**
             * System location code will not have node number and any other
             * details.
             */
            l_unexpandedLocationCode = "Umts";
        }
        // The given location code is neither "fcs" or "mts".
        else
        {
            phosphor::logging::elog<types::DbusInvalidArgument>(
                types::InvalidArgument::ARGUMENT_NAME("LOCATIONCODE"),
                types::InvalidArgument::ARGUMENT_VALUE(
                    i_expandedLocationCode.c_str()));
        }
    }

    return std::make_tuple(l_unexpandedLocationCode, l_nodeNummber);
}

types::ListOfPaths Manager::getFrusByExpandedLocationCode(
    const std::string& i_expandedLocationCode)
{
    std::tuple<std::string, uint16_t> l_locationAndNodePair =
        getUnexpandedLocationCode(i_expandedLocationCode);

    return getFrusByUnexpandedLocationCode(std::get<0>(l_locationAndNodePair),
                                           std::get<1>(l_locationAndNodePair));
}

void Manager::registerHostStateChangeCallback()
{
    static std::shared_ptr<sdbusplus::bus::match_t> l_hostState =
        std::make_shared<sdbusplus::bus::match_t>(
            *m_asioConnection,
            sdbusplus::bus::match::rules::propertiesChanged(
                constants::hostObjectPath, constants::hostInterface),
            [this](sdbusplus::message_t& i_msg) {
                hostStateChangeCallBack(i_msg);
            });
}

void Manager::hostStateChangeCallBack(sdbusplus::message_t& i_msg)
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw std::runtime_error(
                "Error reading callback message for host state");
        }

        std::string l_objectPath;
        types::PropertyMap l_propMap;
        i_msg.read(l_objectPath, l_propMap);

        const auto l_itr = l_propMap.find("CurrentHostState");

        if (l_itr == l_propMap.end())
        {
            throw std::runtime_error(
                "CurrentHostState field is missing in callback message");
        }

        if (auto l_hostState = std::get_if<std::string>(&(l_itr->second)))
        {
            // implies system is moving from standby to power on state
            if (*l_hostState == "xyz.openbmc_project.State.Host.HostState."
                                "TransitioningToRunning")
            {
                // TODO: check for all the essential FRUs in the system.

                // Perform recollection.
                performVpdRecollection();
                return;
            }
        }
        else
        {
            throw std::runtime_error(
                "Invalid type recieved in variant for host state.");
        }
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Log PEL.
        logging::logMessage(l_ex.what());
    }
}

void Manager::performVpdRecollection()
{
    try
    {
        if (m_worker.get() != nullptr)
        {
            nlohmann::json l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();

            // Check if system config JSON is present
            if (l_sysCfgJsonObj.empty())
            {
                throw std::runtime_error(
                    "System config json object is empty, can't process recollection.");
            }

            const auto& l_frusReplaceableAtStandby =
                jsonUtility::getListOfFrusReplaceableAtStandby(l_sysCfgJsonObj);

            for (const auto& l_fruInventoryPath : l_frusReplaceableAtStandby)
            {
                // ToDo: Add some logic/trace to know the flow to
                // collectSingleFruVpd has been directed via
                // performVpdRecollection.
                collectSingleFruVpd(l_fruInventoryPath);
            }
            return;
        }

        throw std::runtime_error(
            "Worker object not found can't process recollection");
    }
    catch (const std::exception& l_ex)
    {
        // TODO Log PEL
        logging::logMessage(
            "VPD recollection failed with error: " + std::string(l_ex.what()));
    }
}
} // namespace vpd
