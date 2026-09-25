#include "config.h"

#include "ibm_handler.hpp"

#include "configuration.hpp"
#include "listener.hpp"
#include "logger.hpp"
#include "parser.hpp"
#include "worker.hpp"

#include <gpiod.hpp>
#include <utility/common_utility.hpp>
#include <utility/dbus_utility.hpp>
#include <utility/json_utility.hpp>
#include <utility/vpd_specific_utility.hpp>

#include <unordered_set>

namespace vpd
{
IbmHandler::IbmHandler(
    std::shared_ptr<BackupAndRestore>& backupAndRestoreObj,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& iFace,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& progressIFace,
    const std::shared_ptr<boost::asio::io_context>& ioCon,
    const std::shared_ptr<sdbusplus::asio::connection>& asioConnection,
    const types::VpdCollectionMode& vpdCollectionMode) :
    backupAndRestoreObj(backupAndRestoreObj), interface(iFace),
    progressInterface(progressIFace), ioContext(ioCon),
    asioConnection(asioConnection), logger(Logger::getLoggerInstance()),
    vpdCollectionMode(vpdCollectionMode)
{
    try
    {
        // validate the VPD collection mode
        validateVpdCollectionMode();

        // check if symlink is present
        isSymlinkPresent();

        // Set up minimal things that is needed before bus name is claimed.
        performInitialSetup();

        // Init back up and restore.
        initBackupAndRestore();
    }
    catch (const std::exception& exception)
    {
        // PEL must have been logged if the code is at this point. So no need to
        // log again. Let the service continue to execute.
        logger->logMessage("IBM Handler instantiation failed. Reason: " +
                           std::string(exception.what()));
    }
}

void IbmHandler::isSymlinkPresent() noexcept
{
    // Check if symlink is already there to confirm fresh boot/factory reset.
    std::error_code ec;
    if (!std::filesystem::exists(INVENTORY_JSON_SYM_LINK, ec))
    {
        if (ec)
        {
            logger->logMessage(
                "Error reading symlink location. Reason: " + ec.message());
        }

        if (dbusUtility::isChassisPowerOn())
        {
            // Predictive PEL logged. Symlink can't go missing while chassis
            // is on as system VPD will not get processed in chassis on state.

            logger->logMessage(
                std::string(
                    "Error reading config JSON symlink in chassis on state."),
                PlaceHolder::PEL,
                types::PelInfoTuple{types::ErrorType::FirmwareError,
                                    types::SeverityType::Warning, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt, std::nullopt});
        }
        return;
    }

    logger->logMessage("Sym Link present.");

    // update JSON path to symlink path.
    configJsonPath = INVENTORY_JSON_SYM_LINK;
    symlinkPresent = true;
}

void IbmHandler::initBackupAndRestore() noexcept
{
    try
    {
        uint16_t errCode = 0;

        // If the object is already there, implies back up and restore took
        // place in initial set up flow.
        if ((backupAndRestoreObj == nullptr))
        {
            if (sysCfgJsonObj.empty())
            {
                // Throwing as sysconfig JSON empty is not expected at this
                // point of execution and also not having backup and restore
                // object will effect system VPD sync.
                throw std::runtime_error(
                    "sysconfig JSON found empty while initializing back up and restore object. JSON path: " +
                    configJsonPath);
            }

            if (!jsonUtility::isBackupAndRestoreRequired(errCode))
            {
                if (errCode)
                {
                    // Throwing as setting of error code confirms that back up
                    // and restore object will not get initialized. This will
                    // effect system VPD sync.
                    throw std::runtime_error(
                        "Failed to check if backup & restore required. Error : " +
                        commonUtility::getErrCodeMsg(errCode));
                }

                // Implies backup and restore not required.
                return;
            }

            backupAndRestoreObj =
                std::make_shared<BackupAndRestore>(sysCfgJsonObj);
        }
    }
    catch (const std::exception& exception)
    {
        // PEL logged as system VPD sync will be affected without this
        // feature.

        logger->logMessage(
            std::string("Back up and restore instantiation failed.") +
                EventLogger::getErrorMsg(exception),
            PlaceHolder::PEL,
            types::PelInfoTuple{EventLogger::getErrorType(exception),
                                types::SeverityType::Warning, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});
    }
}

void IbmHandler::initIbmListenerObject(
    std::shared_ptr<Listener>& eventListener) noexcept
{
    try
    {
        this->eventListener = eventListener;

        if (!this->eventListener)
        {
            logger->logMessage(
                "Event listener is not initialized. Cannot register callbacks.");
            return;
        }

        this->eventListener->registerAssetTagChangeCallback();
        this->eventListener->registerHostStateChangeCallback();
        this->eventListener->registerPresenceChangeCallback();
        this->eventListener->registerCollectionStatusChangeCallback(
            [this](sdbusplus::message_t& msg) {
                collectionStatusChangeCallback(msg);
            });
    }
    catch (const std::exception& exception)
    {
        logger->logMessage("Failed to initialize event listener. Error: " +
                           std::string(exception.what()));
    }
}

void IbmHandler::enableMuxChips()
{
    if (sysCfgJsonObj.empty())
    {
        // config JSON should not be empty at this point of execution.
        throw std::runtime_error("Config JSON is empty. Can't enable muxes");
        return;
    }

    if (!sysCfgJsonObj.contains("muxes"))
    {
        logger->logMessage("No mux defined for the system in config JSON");
        return;
    }

    // iterate over each MUX detail and enable them.
    for (const auto& item : sysCfgJsonObj["muxes"])
    {
        uint16_t errCode = 0;
        if (item.contains("holdidlepath"))
        {
            std::string cmd = "echo 0 > ";
            cmd += item["holdidlepath"];

            logger->logMessage("Enabling mux with command = " + cmd);

            commonUtility::executeCmd(cmd, errCode);

            if (errCode)
            {
                logger->logMessage(
                    "Failed to execute command [" + cmd +
                    "], error : " + commonUtility::getErrCodeMsg(errCode));
            }

            continue;
        }

        logger->logMessage(
            "Mux Entry does not have hold idle path. Can't enable the mux");
    }
}

void IbmHandler::getSystemJson(std::string& systemJson,
                               const types::VPDMapVariant& parsedVpdMap)
{
    if (auto pVal = std::get_if<types::IPZVpdMap>(&parsedVpdMap))
    {
        uint16_t errCode = 0;
        std::string hwKwdValue =
            vpdSpecificUtility::getHWVersion(*pVal, errCode);
        if (hwKwdValue.empty())
        {
            if (errCode)
            {
                throw DataException("Failed to fetch HW value. Reason: " +
                                    commonUtility::getErrCodeMsg(errCode));
            }
            throw DataException("HW value fetched is empty.");
        }

        const std::string& imKwdValue =
            vpdSpecificUtility::getIMValue(*pVal, errCode);
        if (imKwdValue.empty())
        {
            if (errCode)
            {
                throw DataException("Failed to fetch IM value. Reason: " +
                                    commonUtility::getErrCodeMsg(errCode));
            }
            throw DataException("IM value fetched is empty.");
        }

        auto itrToIm = config::systemType.find(imKwdValue);
        if (itrToIm == config::systemType.end())
        {
            throw DataException("IM keyword does not map to any system type");
        }

        const types::HWVerList hwVersionList = itrToIm->second.second;
        if (!hwVersionList.empty())
        {
            transform(hwKwdValue.begin(), hwKwdValue.end(),
                      hwKwdValue.begin(), ::toupper);

            auto itrToHw =
                std::find_if(hwVersionList.begin(), hwVersionList.end(),
                             [&hwKwdValue](const auto& aPair) {
                                 return aPair.first == hwKwdValue;
                             });

            if (itrToHw != hwVersionList.end())
            {
                if (!(*itrToHw).second.empty())
                {
                    systemJson += (*itrToIm).first + "_" +
                                  (*itrToHw).second + ".json";
                }
                else
                {
                    systemJson += (*itrToIm).first + ".json";
                }
                return;
            }
        }
        systemJson += itrToIm->second.first + ".json";
        return;
    }

    throw DataException(
        "Invalid VPD type returned from Parser. Can't get system JSON.");
}

void IbmHandler::setEnvAndReboot(const std::string& key,
                                 const std::string& value)
{
    // set env and reboot and break.
    uint16_t errCode = 0;
    commonUtility::executeCmd("/sbin/fw_setenv", errCode, key, value);

    if (errCode)
    {
        throw std::runtime_error(
            "Failed to execute command [/sbin/fw_setenv " + key + " " +
            value + "], error : " + commonUtility::getErrCodeMsg(errCode));
    }

#ifdef SKIP_REBOOT_ON_FITCONFIG_CHANGE
    logger->logMessage("NOT Rebooting BMC to pick up new device tree");
#else
    logger->logMessage("Rebooting BMC to pick up new device tree");

    // make dbus call to reboot
    auto bus = sdbusplus::bus::new_default_system();
    auto method = bus.new_method_call(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "Reboot");
    bus.call_noreply(method);
    exit(EXIT_SUCCESS);
#endif
}

std::string IbmHandler::readFitConfigValue()
{
    uint16_t errCode = 0;
    std::vector<std::string> output =
        commonUtility::executeCmd("/sbin/fw_printenv", errCode);

    if (errCode)
    {
        logger->logMessage(
            "Failed to execute command [/sbin/fw_printenv], error : " +
            commonUtility::getErrCodeMsg(errCode));
    }

    std::string fitConfigValue;

    for (const auto& entry : output)
    {
        auto pos = entry.find("=");
        auto key = entry.substr(0, pos);
        if (key != "fitconfig")
        {
            continue;
        }

        if (pos + 1 < entry.size())
        {
            fitConfigValue = entry.substr(pos + 1);
        }
    }

    return fitConfigValue;
}

bool IbmHandler::isBackupOnCache()
{
    try
    {
        uint16_t errCode = 0;
        std::string backupAndRestoreCfgFilePath =
            sysCfgJsonObj.value("backupRestoreConfigPath", "");

        if (backupAndRestoreCfgFilePath.empty())
        {
            logger->logMessage(
                "backupRestoreConfigPath is not found in JSON. Can't determine the backup path.");
            return false;
        }

        nlohmann::json backupAndRestoreCfgJsonObj =
            jsonUtility::getParsedJson(backupAndRestoreCfgFilePath,
                                       errCode);
        if (backupAndRestoreCfgJsonObj.empty() || errCode)
        {
            logger->logMessage(
                "JSON parsing failed for file [ " +
                std::string(backupAndRestoreCfgFilePath) +
                " ], error : " + commonUtility::getErrCodeMsg(errCode));
            return false;
        }

        // check if either of "source" or "destination" has inventory path.
        // this indicates that this system has System VPD on hardware
        // and other copy on D-Bus (BMC cache).
        if (!backupAndRestoreCfgJsonObj.empty() &&
            ((backupAndRestoreCfgJsonObj.contains("source") &&
              backupAndRestoreCfgJsonObj["source"].contains(
                  "inventoryPath")) ||
             (backupAndRestoreCfgJsonObj.contains("destination") &&
              backupAndRestoreCfgJsonObj["destination"].contains(
                  "inventoryPath"))))
        {
            return true;
        }
    }
    catch (const std::exception& exception)
    {
        logger->logMessage(
            "Exception while checking for backup on cache. Reason:" +
            std::string(exception.what()));
    }

    // In case of any failure/ambiguity. Don't perform back up and restore.
    return false;
}

void IbmHandler::performBackupAndRestore(types::VPDMapVariant& srcVpdMap)
{
    try
    {
        backupAndRestoreObj =
            std::make_shared<BackupAndRestore>(sysCfgJsonObj);
        auto [srcVpdVariant,
              dstVpdVariant] = backupAndRestoreObj->backupAndRestore();

        // ToDo: Revisit is this check is required or not.
        if (auto srcVpdMapPtr = std::get_if<types::IPZVpdMap>(&srcVpdVariant);
            srcVpdMapPtr && !(*srcVpdMapPtr).empty())
        {
            srcVpdMap = std::move(srcVpdVariant);
        }
    }
    catch (const std::exception& exception)
    {
        logger->logMessage(
            std::format(
                "Exception caught while backup and restore VPD keywords. Reason: {}",
                EventLogger::getErrorMsg(exception)),
            PlaceHolder::ASYNC_PEL,
            types::PelInfoTuple{EventLogger::getErrorType(exception),
                                types::SeverityType::Warning, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});
    }
}

std::string IbmHandler::createAssetTagString(
    const types::VPDMapVariant& parsedVpdMap)
{
    std::string assetTag;
    // system VPD will be in IPZ format.
    if (auto parsedVpdMapPtr = std::get_if<types::IPZVpdMap>(&parsedVpdMap))
    {
        auto itrToVsys = (*parsedVpdMapPtr).find(constants::recVSYS);
        if (itrToVsys != (*parsedVpdMapPtr).end())
        {
            uint16_t errCode = 0;
            const std::string tmKwdValue{vpdSpecificUtility::getKwVal(
                itrToVsys->second, constants::kwdTM, errCode)};
            if (tmKwdValue.empty())
            {
                throw std::runtime_error(
                    std::string("Failed to get value for keyword [") +
                    constants::kwdTM +
                    std::string("] while creating Asset tag. Error : " +
                                commonUtility::getErrCodeMsg(errCode)));
            }
            const std::string seKwdValue{vpdSpecificUtility::getKwVal(
                itrToVsys->second, constants::kwdSE, errCode)};
            if (seKwdValue.empty())
            {
                throw std::runtime_error(
                    std::string("Failed to get value for keyword [") +
                    constants::kwdSE +
                    std::string("] while creating Asset tag. Error : " +
                                commonUtility::getErrCodeMsg(errCode)));
            }
            assetTag = std::string{"Server-"} + tmKwdValue +
                       std::string{"-"} + seKwdValue;
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
            "Invalid VPD type received to create Asset tag.");
    }
    return assetTag;
}

void IbmHandler::publishSystemVPD(const types::VPDMapVariant& parsedVpdMap)
{
    types::ObjectMap objectInterfaceMap;
    if (std::get_if<types::IPZVpdMap>(&parsedVpdMap))
    {
        Worker{}.populateDbus(sysCfgJsonObj, parsedVpdMap,
                              objectInterfaceMap, SYSTEM_VPD_FILE_PATH);

        // In split mode system, file mode system VPD has to be enabled.
        // Update system inventory for split mode
        if (vpdCollectionMode == types::VpdCollectionMode::FILE_MODE)
        {
            resetNonSystemInvPaths(objectInterfaceMap);
        }

        try
        {
            if (isFactoryResetDone)
            {
                const auto& assetTag = createAssetTagString(parsedVpdMap);
                auto itrToSystemPath = objectInterfaceMap.find(
                    sdbusplus::object_path(constants::systemInvPath));
                if (itrToSystemPath == objectInterfaceMap.end())
                {
                    throw std::runtime_error(
                        "Asset tag update failed. System Path not found in object map.");
                }
                types::PropertyMap assetTagProperty;
                assetTagProperty.emplace("AssetTag", assetTag);
                (itrToSystemPath->second)
                    .emplace(constants::assetTagInf,
                             std::move(assetTagProperty));
            }
        }
        catch (const std::exception& exception)
        {
            logger->logMessage(
                std::format(
                    "Exception caught while updating Asset Tag. Error {}",
                    EventLogger::getErrorMsg(exception)),
                PlaceHolder::ASYNC_PEL,
                types::PelInfoTuple{EventLogger::getErrorType(exception),
                                    types::SeverityType::Warning, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt, std::nullopt});
        }

        addOrRestoreAvailableProperty(objectInterfaceMap);

        // Call method to update the dbus
        if (!dbusUtility::publishVpdOnDBus(std::move(objectInterfaceMap)))
        {
            throw std::runtime_error("Call to PIM failed for system VPD");
        }
    }
    else
    {
        throw DataException("Invalid format of parsed VPD map.");
    }
}

void IbmHandler::setJsonSymbolicLink(const std::string& systemJson)
{
    std::error_code ec;
    ec.clear();

    // Check if symlink file path exists and if the JSON at this location is a
    // symlink.
    if (symlinkPresent &&
        std::filesystem::is_symlink(INVENTORY_JSON_SYM_LINK, ec))
    {
        // Don't care about exception in "is_symlink". Will continue with
        // creation of symlink.
        const auto& symlinkFilePath =
            std::filesystem::read_symlink(INVENTORY_JSON_SYM_LINK, ec);
        if (ec)
        {
            logger->logMessage(
                "Can't read existing symlink. Error =" + ec.message() +
                "Trying removal of symlink and creation of new symlink.");
        }

        // If currently set JSON is the required one. No further processing
        // required.
        if (systemJson == symlinkFilePath)
        {
            // Correct symlink is already set.
            return;
        }

        if (!std::filesystem::remove(INVENTORY_JSON_SYM_LINK, ec))
        {
            // No point going further. If removal fails for existing symlink,
            // create will anyways throw.
            throw std::runtime_error(
                "Removal of symlink failed with Error = " + ec.message() +
                ". Can't proceed with create_symlink.");
        }
    }

    if (!std::filesystem::exists(VPD_SYMLIMK_PATH, ec))
    {
        if (ec)
        {
            throw std::runtime_error(
                "File system call to exist failed with error = " +
                ec.message());
        }

        // implies it is a fresh boot/factory reset.
        // Create the directory for hosting the symlink
        if (!std::filesystem::create_directories(VPD_SYMLIMK_PATH, ec))
        {
            if (ec)
            {
                throw std::runtime_error(
                    "File system call to create directory failed with error = " +
                    ec.message());
            }
        }
    }

    // create a new symlink based on the system
    std::filesystem::create_symlink(systemJson, INVENTORY_JSON_SYM_LINK,
                                    ec);
    if (ec)
    {
        throw std::runtime_error(
            "create_symlink system call failed with error: " + ec.message());
    }

    // update path to symlink.
    configJsonPath = INVENTORY_JSON_SYM_LINK;
    symlinkPresent = true;

    // If the flow is at this point implies the symlink was not present there.
    // Considering this as factory reset.
    isFactoryResetDone = true;
}

void IbmHandler::setDeviceTreeAndJson(
    const std::string& fruPath, types::VPDMapVariant& parsedSystemVpdMap)
{
    // JSON is mandatory for processing of this API.
    if (sysCfgJsonObj.empty())
    {
        throw JsonException("System config JSON is empty", sysCfgJsonObj);
    }

    static std::string error;
    uint16_t errCode = 0;
    try
    {
        std::string systemVpdPath{fruPath};
        commonUtility::getEffectiveFruPath(vpdCollectionMode, systemVpdPath,
                                           errCode);

        if (errCode)
        {
            throw std::runtime_error(
                "Failed to get effective System VPD path, for [" +
                systemVpdPath +
                "], reason: " + commonUtility::getErrCodeMsg(errCode));
        }

        // parse system VPD
        std::shared_ptr<Parser> vpdParser =
            std::make_shared<Parser>(systemVpdPath, sysCfgJsonObj);
        parsedSystemVpdMap = vpdParser->parse();

        if (std::holds_alternative<std::monostate>(parsedSystemVpdMap))
        {
            throw std::runtime_error("VPD parsing failed");
        }
    }
    catch (const std::exception& exception)
    {
        error += std::format(
            "System VPD collection failed from path [{}], reason: {}. ",
            fruPath, exception.what());

        // if system is in split mode, and we failed to collect VPD from primary
        // EEPROM file path, do not attempt to collect from redundant EEPROM
        // file path.
        if (vpdCollectionMode == types::VpdCollectionMode::FILE_MODE)
        {
            logger->logMessage(error);
            throw EepromException(error);
        }

        const std::string& redundantEepromPath{
            REDUNDANT_SYSTEM_VPD_FILE_PATH};

        if (redundantEepromPath.empty() || redundantEepromPath == fruPath)
        {
            logger->logMessage(error);
            throw EepromException(error);
        }

        // Try system VPD collection from redundant path
        setDeviceTreeAndJson(redundantEepromPath, parsedSystemVpdMap);

        return;
    }

    /* TODO: Revisit the code and update the flow will specific error handling
       so that specific failure type can be reported in the PEL.

       Also, as per current flow in case redundant path collection fails post
       this point, current implementation will end up logging two PELs which is
       not desired and needs to be handled. Update code to log only one PEL
       irrespective to any failure in the flow.*/
    if (fruPath != SYSTEM_VPD_FILE_PATH)
    {
        // TODO: Replace with a device callout once the corresponding API
        // is implemented.
        logger->logMessage(
            error +
                std::format(
                    " Successfully collected VPD from redundant path [{}].",
                    fruPath),
            PlaceHolder::ASYNC_PEL_WITH_INV_CALLOUT,
            types::PelInfoTuple{
                types::ErrorType::FirmwareError, types::SeverityType::Warning,
                0, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                std::optional<types::CalloutData>{types::InventoryCalloutData{
                    SYSTEM_VPD_FILE_PATH, types::CalloutPriority::High}}});
    }

    // Implies it is default JSON.
    std::string systemJson{JSON_ABSOLUTE_PATH_PREFIX};

    // get system JSON as per the system configuration.
    getSystemJson(systemJson, parsedSystemVpdMap);

    if (!systemJson.compare(JSON_ABSOLUTE_PATH_PREFIX))
    {
        throw DataException(
            "No system JSON found corresponding to IM read from VPD.");
    }

    // re-parse the JSON once appropriate JSON has been selected.
    sysCfgJsonObj = jsonUtility::getParsedJson(systemJson, errCode);

    if (errCode)
    {
        throw(JsonException(
            "JSON parsing failed for file [ " + systemJson +
                " ], error : " + commonUtility::getErrCodeMsg(errCode),
            systemJson));
    }

    vpdSpecificUtility::setCollectionStatusProperty(
        SYSTEM_VPD_FILE_PATH, types::VpdCollectionStatus::InProgress,
        sysCfgJsonObj, errCode);

    if (errCode)
    {
        logger->logMessage("Failed to set collection status for path " +
                           std::string(SYSTEM_VPD_FILE_PATH) + "Reason: " +
                           commonUtility::getErrCodeMsg(errCode));
    }

    std::vector<std::string> devTreesFromJson;
    if (sysCfgJsonObj.contains("devTree"))
    {
        const auto& devTreeVal = sysCfgJsonObj["devTree"];
        if (devTreeVal.is_array())
        {
            devTreesFromJson = devTreeVal.get<std::vector<std::string>>();
        }
        else if (devTreeVal.is_string())
        {
            devTreesFromJson.emplace_back(devTreeVal.get<std::string>());
        }

        if (devTreesFromJson.empty())
        {
            logger->logMessage(
                std::format(
                    "Mandatory value for device tree missing from JSON[{}]",
                    systemJson),
                PlaceHolder::ASYNC_PEL,
                types::PelInfoTuple{types::ErrorType::JsonFailure,
                                    types::SeverityType::Error, 0, std::nullopt,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt});
        }
    }

    auto fitConfigVal = readFitConfigValue();

    // Check if any of the device trees from JSON is already set in fitconfig.
    const bool devTreeAlreadySet = std::any_of(
        devTreesFromJson.begin(), devTreesFromJson.end(),
        [&fitConfigVal](const std::string& devTree) {
            return fitConfigVal.find(devTree) != std::string::npos;
        });

    if (devTreesFromJson.empty() || devTreeAlreadySet)
    {
        // Skipping setting device tree as either devtree info is missing from
        // Json or it is rightly set.

        setJsonSymbolicLink(systemJson);

        const std::string& sysVpdInvPath =
            jsonUtility::getInventoryObjPathFromJson(SYSTEM_VPD_FILE_PATH,
                                                     errCode);

        if (sysVpdInvPath.empty())
        {
            if (errCode)
            {
                throw JsonException(
                    "System vpd inventory path not found in JSON. Reason:" +
                        commonUtility::getErrCodeMsg(errCode),
                    INVENTORY_JSON_SYM_LINK);
            }
            throw JsonException("System vpd inventory path is missing in JSON",
                                INVENTORY_JSON_SYM_LINK);
        }

        // TODO: for backward compatibility this should also support motherboard
        // interface.
        std::vector<std::string> interfaceList{
            constants::motherboardInterface};
        const types::MapperGetObject& sysVpdObjMap =
            dbusUtility::getObjectMap(sysVpdInvPath, interfaceList);

        if (!sysVpdObjMap.empty())
        {
            if (isBackupOnCache() &&
                jsonUtility::isBackupAndRestoreRequired(errCode))
            {
                performBackupAndRestore(parsedSystemVpdMap);
            }
            else if (errCode)
            {
                logger->logMessage(
                    "Failed to check if backup and restore required. Reason : " +
                    commonUtility::getErrCodeMsg(errCode));
            }
        }
        return;
    }

    logger->logMessage("Set fitconfig and reboot from VPD-Manager");

    // Use the first entry in the array as the preferred device tree to set as
    // IBM-Handler does not have the intelligence to choose from the list.
    // In case of list of dts, HE-App will choose the required dts.
    setEnvAndReboot("fitconfig", devTreesFromJson.front());
#ifdef SKIP_REBOOT_ON_FITCONFIG_CHANGE
    setJsonSymbolicLink(systemJson);
#endif
}

void IbmHandler::performInitialSetup()
{
    // Parse whatever JSON is set as of now.
    uint16_t errCode = 0;
    try
    {
        sysCfgJsonObj =
            jsonUtility::getParsedJson(configJsonPath, errCode);

        if (errCode)
        {
            // Throwing as there is no point proceeding without any JSON.
            throw JsonException("JSON parsing failed. error : " +
                                    commonUtility::getErrCodeMsg(errCode),
                                configJsonPath);
        }

        types::VPDMapVariant parsedSysVpdMap;
        setDeviceTreeAndJson(SYSTEM_VPD_FILE_PATH, parsedSysVpdMap);

        // proceed to publish system VPD.
        publishSystemVPD(parsedSysVpdMap);

        vpdSpecificUtility::setCollectionStatusProperty(
            SYSTEM_VPD_FILE_PATH, types::VpdCollectionStatus::Completed,
            sysCfgJsonObj, errCode);

        if (errCode)
        {
            logger->logMessage(
                "Failed to set collection status for path " +
                std::string(SYSTEM_VPD_FILE_PATH) +
                "Reason: " + commonUtility::getErrCodeMsg(errCode));
        }

        // Enable all mux which are used for connecting to the i2c on the
        // pcie slots for pcie cards. These are not enabled by kernel due to
        // an issue seen with Castello cards, where the i2c line hangs on a
        // probe.
        enableMuxChips();

        // Nothing needs to be done. Service restarted or BMC re-booted for
        // some reason at system power on.
    }
    catch (const std::exception& exception)
    {
        // Setting of collection status should be utility method
        vpdSpecificUtility::setCollectionStatusProperty(
            SYSTEM_VPD_FILE_PATH, types::VpdCollectionStatus::Failed,
            sysCfgJsonObj, errCode);

        if (errCode)
        {
            logger->logMessage(
                "Failed to set collection status for path " +
                std::string(SYSTEM_VPD_FILE_PATH) +
                "Reason: " + commonUtility::getErrCodeMsg(errCode));
        }

        // Any issue in system's initial set up is handled in this catch. Error
        // will not propagate to manager.

        std::optional<types::CalloutData> callout = std::nullopt;
        PlaceHolder placeHolder = PlaceHolder::ASYNC_PEL;

        if (typeid(exception) == typeid(EepromException))
        {
            placeHolder = PlaceHolder::ASYNC_PEL_WITH_INV_CALLOUT;
            callout =
                types::InventoryCalloutData{std::string(SYSTEM_VPD_FILE_PATH),
                                            types::CalloutPriority::High};
        }

        logger->logMessage(
            std::format("Exception while performing initial set up. Error: {}",
                        EventLogger::getErrorMsg(exception)),
            placeHolder,
            types::PelInfoTuple{EventLogger::getErrorType(exception),
                                types::SeverityType::Critical, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                callout});
    }
}

void IbmHandler::collectionStatusChangeCallback(
    sdbusplus::message_t& msg) const noexcept
{
    try
    {
        if (msg.is_method_error())
        {
            throw std::runtime_error(
                "Error reading callback message for collection status");
        }

        std::string callbackInterface;
        types::PropertyMap propMap;
        msg.read(callbackInterface, propMap);

        const auto itr = propMap.find("Status");
        if (itr == propMap.end())
        {
            logger->logMessage("Status property missing in property map. "
                               "Returning without processing.");
            return;
        }

        const auto status = std::get_if<std::string>(&(itr->second));
        if (status == nullptr)
        {
            throw std::runtime_error(
                "Invalid type received in variant for collection status");
        }

        if (vpd::types::CommonProgress::convertOperationStatusFromString(
                *status) == types::VpdCollectionStatus::Completed ||
            vpd::types::CommonProgress::convertOperationStatusFromString(
                *status) == types::VpdCollectionStatus::Failed)
        {
            if (backupAndRestoreObj)
            {
                backupAndRestoreObj->backupAndRestore();
            }

            if (eventListener)
            {
                // Check if system config JSON specifies
                // correlatedPropertiesJson
                if (sysCfgJsonObj.contains("correlatedPropertiesConfigPath"))
                {
                    // register correlated properties callback with specific
                    // correlated properties JSON
                    eventListener->registerCorrPropCallBack(
                        sysCfgJsonObj["correlatedPropertiesConfigPath"]
                            .get<std::string>());
                }
                else
                {
                    logger->logMessage(
                        "Correlated properties JSON path is not defined in system config JSON. Correlated properties listener is disabled.");
                }
            }

            if (constants::FAILURE == handleBmcReadyToRemove())
            {
                logger->logMessage(
                    "Failed to handle ReadyToRemove property for BMC");
            }
        }
    }
    catch (const std::exception& exception)
    {
        logger->logMessage(
            std::format("Collection status change callback failed, reason: {}",
                        exception.what()));
    }
}

void IbmHandler::updateVpdCollectionStatus(
    const types::VpdCollectionStatus status) const noexcept
{
    progressInterface->set_property(
        "Status",
        types::CommonProgress::convertOperationStatusToString(status));
}

void IbmHandler::addOrRestoreAvailableProperty(
    types::ObjectMap& objectInterfaceMap)
{
    try
    {
        for (auto& [inventoryPath, interfaceMap] : objectInterfaceMap)
        {
            auto mapperObjectMap = dbusUtility::getObjectMap(
                inventoryPath.str, {constants::availabilityInf});

            // If property exists under PIM, skip this inventory path
            auto it =
                std::find_if(mapperObjectMap.begin(), mapperObjectMap.end(),
                             [](const auto& pair) {
                                 return pair.first == constants::pimServiceName;
                             });
            if (it != mapperObjectMap.end())
            {
                // The object is already under PIM. No need to process
                // again. Retain the old value
                continue;
            }

            // Property doesn't exist on D-Bus. Populate it with default
            // value "false".
            types::PropertyMap availableProperty;
            availableProperty.emplace(constants::availableProperty, false);
            interfaceMap.emplace(constants::availabilityInf,
                                 std::move(availableProperty));
        }
    }
    catch (const std::exception& exception)
    {
        // TODO: Can we have "&" option in logMessage API in case we want to
        // log at multiple places?
        logger->logMessage(std::format(
            "Exception caught while updating Available property. Error {}",
            EventLogger::getErrorMsg(exception)));
    }
}

void IbmHandler::resetNonSystemInvPaths(
    types::ObjectMap& objectMap) const noexcept
{
    try
    {
        // For all inventory paths other than system inventory path:
        // 1. Preserve the existing interfaces and properties in the map
        // 2. Reset the existing properties in the map to default values
        for (auto& [path, interfaceMap] : objectMap)
        {
            // Skip the system inventory path - keep it unchanged
            if (path == sdbusplus::object_path(constants::systemInvPath))
            {
                continue;
            }

            uint16_t errCode = 0;
            types::InterfaceMap resetInterfaceMap;

            vpdSpecificUtility::resetDataUnderPIM(
                path.str, resetInterfaceMap, true, errCode);

            if (errCode)
            {
                logger->logMessage(std::format(
                    "Failed to reset data for path [{}], error: {}. Skipping.",
                    path.str, commonUtility::getErrCodeMsg(errCode)));
                continue;
            }

            // Replace the interface map with reset data
            interfaceMap = std::move(resetInterfaceMap);
        }
    }
    catch (const std::exception& exception)
    {
        logger->logMessage(std::format(
            "Error while filtering system VPD map for non system inventory path: {}",
            exception.what()));
    }
}

void IbmHandler::validateVpdCollectionMode() const
{
    if (vpdCollectionMode == types::VpdCollectionMode::FILE_MODE)
    {
        // check if the system VPD EEPROM is accessible
        //  @todo:  need to replace this EEPROM check with patch panel presence
        //  check by
        // reading it from cable management service when the service is ready
        std::error_code ec;
        if (std::filesystem::exists(SYSTEM_VPD_FILE_PATH, ec))
        {
            // log a critical PEL
            logger->logMessage(
                "Patch panel is accessible while the system is in file mode. Please check the system configuration.",
                PlaceHolder::ASYNC_PEL,
                types::PelInfoTuple{types::ErrorType::FirmwareError,
                                    types::SeverityType::Critical, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt, std::nullopt});

            // throw exception to prevent system VPD collection
            throw FirmwareException(
                "Patch panel is accessible while the system is in file mode. Please check the system configuration.");
        }

        //@todo: remove below log message once the patch panel presence check is
        // implemented
        if (ec)
        {
            logger->logMessage(std::format(
                "Failed to check if system VPD EEPROM is accessible, error: {}. Continuing system VPD collection in file mode.",
                ec.message()));
        }
    }
}

int IbmHandler::handleBmcReadyToRemove() const noexcept
{
    int retVal{constants::FAILURE};
    try
    {
        // Read BMC position to identify the passive (sibling) BMC.
        const auto variantPosition = dbusUtility::readDbusProperty(
            constants::pimServiceName, constants::systemVpdInvPath,
            constants::positionInterface, constants::positionPropertyName);

        const auto* bmcPosition = std::get_if<size_t>(&variantPosition);
        if (!bmcPosition)
        {
            logger->logMessage(std::format(
                "Invalid BMC position data type read from D-Bus. Cannot process "
                "ReadyToRemove property."));

            return retVal;
        }

        if (*bmcPosition != constants::VALUE_0 &&
            *bmcPosition != constants::VALUE_1)
        {
            logger->logMessage(std::format(
                "Invalid BMC position {} read from D-Bus. Cannot process "
                "ReadyToRemove property.",
                *bmcPosition));

            return retVal;
        }

        // get the BMC inventory paths
        const auto bmcInvPathsResult = dbusUtility::getBMCInventoryPaths();
        if (!bmcInvPathsResult)
        {
            logger->logMessage(std::format(
                "Failed to get BMC inventory paths from D-Bus. Cannot process "
                "ReadyToRemove property. Error message: {}",
                commonUtility::getErrCodeMsg(bmcInvPathsResult.error())));
            return retVal;
        }

        // check if there are two BMC inventory paths which indicate this system
        // is a redundant BMC system otherwise consider it as a single BMC
        // system and do not process ReadyToRemove interface and property
        if (bmcInvPathsResult.value().size() < constants::VALUE_2)
        {
            logger->logMessage(std::format(
                "Number of BMC inventory paths found {}. Not processing ReadyToRemove property for single BMC system",
                bmcInvPathsResult.value().size()));
            return constants::SUCCESS;
        }

        // Loop through the BMC inventory paths, read the Position property from
        // the Decorator.Position interface, and find the sibling BMC path.
        // The sibling (passive) BMC is the one whose position differs from the
        // active BMC position: position 0 → sibling is position 1, and
        // vice-versa.
        const size_t siblingPosition = (*bmcPosition == constants::VALUE_0)
                                           ? constants::VALUE_1
                                           : constants::VALUE_0;

        std::string passiveBmcInvPath;
        for (const auto& bmcInvPath : bmcInvPathsResult.value())
        {
            const auto positionVariant = dbusUtility::readDbusProperty(
                constants::pimServiceName, bmcInvPath.str,
                constants::positionInterface, constants::positionPropertyName);

            if (const auto* position =
                    std::get_if<size_t>(&positionVariant))
            {
                if (*position == siblingPosition)
                {
                    passiveBmcInvPath = bmcInvPath.str;
                    break;
                }
            }
        }

        if (passiveBmcInvPath.empty())
        {
            logger->logMessage(
                "Passive BMC inventory path is empty. Cannot process "
                "ReadyToRemove property.");
            return retVal;
        }

        // publish ReadyToRemove=false on sibling(passive) BMC's
        // ReadyToRemove interface so that the Concurrent Maintenance flow can
        // identify the sibling(passive) BMC as concurrently maintainable.

        vpd::types::ObjectMap objectMap;
        objectMap[sdbusplus::object_path{passiveBmcInvPath}]
                 [vpd::constants::readyToRemoveIface]
                 [vpd::constants::readyToRemoveProperty] = false;

        if (!dbusUtility::publishVpdOnDBus(std::move(objectMap)))
        {
            logger->logMessage(
                "Failed to publish ReadyToRemove interface on PIM for BMC "
                "inventory path: " +
                passiveBmcInvPath);
            return retVal;
        }

        retVal = constants::SUCCESS;
    }
    catch (const std::exception& exception)
    {
        logger->logMessage(std::format(
            "Failed to handle ReadyToRemove property. Error: {}", exception.what()));
    }
    return retVal;
}

} // namespace vpd
