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
    std::shared_ptr<BackupAndRestore>& o_backupAndRestoreObj,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_iFace,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& i_progressiFace,
    const std::shared_ptr<boost::asio::io_context>& i_ioCon,
    const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection,
    const types::VpdCollectionMode& i_vpdCollectionMode) :
    m_backupAndRestoreObj(o_backupAndRestoreObj), m_interface(i_iFace),
    m_progressInterface(i_progressiFace), m_ioContext(i_ioCon),
    m_asioConnection(i_asioConnection), m_logger(Logger::getLoggerInstance()),
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

            m_logger->logMessage(
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

    m_logger->logMessage("Sym Link present.");

    // update JSON path to symlink path.
    m_configJsonPath = INVENTORY_JSON_SYM_LINK;
    m_isSymlinkPresent = true;
}

void IbmHandler::initBackupAndRestore() noexcept
{
    try
    {
        uint16_t l_errCode = 0;

        // If the object is already there, implies back up and restore took
        // place in initial set up flow.
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

        m_logger->logMessage(
            std::string("Back up and restore instantiation failed.") +
                EventLogger::getErrorMsg(l_ex),
            PlaceHolder::PEL,
            types::PelInfoTuple{EventLogger::getErrorType(l_ex),
                                types::SeverityType::Warning, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});
    }
}

void IbmHandler::initIbmListenerObject(
    std::shared_ptr<Listener>& i_eventListener) noexcept
{
    try
    {
        m_eventListener = i_eventListener;

        if (!m_eventListener)
        {
            m_logger->logMessage(
                "Event listener is not initialized. Cannot register callbacks.");
            return;
        }

        m_eventListener->registerAssetTagChangeCallback();
        m_eventListener->registerHostStateChangeCallback();
        m_eventListener->registerPresenceChangeCallback();
        m_eventListener->registerCollectionStatusChangeCallback(
            [this](sdbusplus::message_t& i_msg) {
                collectionStatusChangeCallback(i_msg);
            });
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage("Failed to initialize event listener. Error: " +
                             std::string(l_ex.what()));
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
        m_logger->logMessage("No mux defined for the system in config JSON");
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

            m_logger->logMessage("Enabling mux with command = " + cmd);

            commonUtility::executeCmd(cmd, l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(
                    "Failed to execute command [" + cmd +
                    "], error : " + commonUtility::getErrCodeMsg(l_errCode));
            }

            continue;
        }

        m_logger->logMessage(
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
        // this indicates that this system has System VPD on hardware
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
        m_logger->logMessage(
            std::format(
                "Exception caught while backup and restore VPD keywords. Reason: {}",
                EventLogger::getErrorMsg(l_ex)),
            PlaceHolder::ASYNC_PEL,
            types::PelInfoTuple{EventLogger::getErrorType(l_ex),
                                types::SeverityType::Warning, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});
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
            "Invalid VPD type received to create Asset tag.");
    }
    return l_assetTag;
}

void IbmHandler::publishSystemVPD(const types::VPDMapVariant& i_parsedVpdMap)
{
    types::ObjectMap l_objectInterfaceMap;
    if (std::get_if<types::IPZVpdMap>(&i_parsedVpdMap))
    {
        Worker{}.populateDbus(m_sysCfgJsonObj, i_parsedVpdMap,
                              l_objectInterfaceMap, SYSTEM_VPD_FILE_PATH);

        // In split mode system, file mode system VPD has to be enabled.
        // Update system inventory for split mode
        if (m_vpdCollectionMode == types::VpdCollectionMode::FILE_MODE)
        {
            resetNonSystemInvPaths(l_objectInterfaceMap);
        }

        try
        {
            if (m_isFactoryResetDone)
            {
                const auto& l_assetTag = createAssetTagString(i_parsedVpdMap);
                auto l_itrToSystemPath = l_objectInterfaceMap.find(
                    sdbusplus::object_path(constants::systemInvPath));
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
            m_logger->logMessage(
                std::format(
                    "Exception caught while updating Asset Tag. Error {}",
                    EventLogger::getErrorMsg(l_ex)),
                PlaceHolder::ASYNC_PEL,
                types::PelInfoTuple{EventLogger::getErrorType(l_ex),
                                    types::SeverityType::Warning, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt, std::nullopt});
        }

        addOrRestoreAvailableProperty(l_objectInterfaceMap);

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
            m_logger->logMessage(
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
    const std::string& i_fruPath, types::VPDMapVariant& o_parsedSystemVpdMap)
{
    // JSON is mandatory for processing of this API.
    if (m_sysCfgJsonObj.empty())
    {
        throw JsonException("System config JSON is empty", m_sysCfgJsonObj);
    }

    static std::string l_error;
    uint16_t l_errCode = 0;
    try
    {
        std::string l_systemVpdPath{i_fruPath};
        commonUtility::getEffectiveFruPath(m_vpdCollectionMode, l_systemVpdPath,
                                           l_errCode);

        if (l_errCode)
        {
            throw std::runtime_error(
                "Failed to get effective System VPD path, for [" +
                l_systemVpdPath +
                "], reason: " + commonUtility::getErrCodeMsg(l_errCode));
        }

        // parse system VPD
        std::shared_ptr<Parser> l_vpdParser =
            std::make_shared<Parser>(l_systemVpdPath, m_sysCfgJsonObj);
        o_parsedSystemVpdMap = l_vpdParser->parse();

        if (std::holds_alternative<std::monostate>(o_parsedSystemVpdMap))
        {
            throw std::runtime_error("VPD parsing failed");
        }
    }
    catch (const std::exception& l_ex)
    {
        l_error += std::format(
            "System VPD collection failed from path [{}], reason: {}. ",
            i_fruPath, l_ex.what());

        const std::string& l_redundantEepromPath{
            REDUNDANT_SYSTEM_VPD_FILE_PATH};

        if (l_redundantEepromPath.empty() || l_redundantEepromPath == i_fruPath)
        {
            m_logger->logMessage(l_error);
            throw EepromException(l_error);
        }

        // Try system VPD collection from redundant path
        setDeviceTreeAndJson(l_redundantEepromPath, o_parsedSystemVpdMap);

        return;
    }

    /* TODO: Revisit the code and update the flow will specific error handling
       so that specific failure type can be reported in the PEL.

       Also, as per current flow in case redundant path collection fails post
       this point, current implementation will end up logging two PELs which is
       not desired and needs to be handled. Update code to log only one PEL
       irrespective to any failure in the flow.*/
    if (i_fruPath != SYSTEM_VPD_FILE_PATH)
    {
        // TODO: Replace with a device callout once the corresponding API
        // is implemented.
        m_logger->logMessage(
            l_error +
                std::format(
                    " Successfully collected VPD from redundant path [{}].",
                    i_fruPath),
            PlaceHolder::ASYNC_PEL_WITH_INV_CALLOUT,
            types::PelInfoTuple{
                types::ErrorType::FirmwareError, types::SeverityType::Warning,
                0, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                std::optional<types::CalloutData>{types::InventoryCalloutData{
                    SYSTEM_VPD_FILE_PATH, types::CalloutPriority::High}}});
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
            m_logger->logMessage(
                std::format(
                    "Mandatory value for device tree missing from JSON[{}]",
                    l_systemJson),
                PlaceHolder::ASYNC_PEL,
                types::PelInfoTuple{types::ErrorType::JsonFailure,
                                    types::SeverityType::Error, 0, std::nullopt,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt});
        }
    }

    auto l_fitConfigVal = readFitConfigValue();

    if (l_devTreeFromJson.empty() ||
        l_fitConfigVal.find(l_devTreeFromJson) != std::string::npos)
    { // Skipping setting device tree as either devtree info is missing from
        // Json or it is rightly set.

        setJsonSymbolicLink(l_systemJson);

        const std::string& l_systemVpdInvPath =
            jsonUtility::getInventoryObjPathFromJson(SYSTEM_VPD_FILE_PATH,
                                                     l_errCode);

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
                m_logger->logMessage(
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
        setDeviceTreeAndJson(SYSTEM_VPD_FILE_PATH, l_parsedSysVpdMap);

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

        // Any issue in system's initial set up is handled in this catch. Error
        // will not propagate to manager.

        std::optional<types::CalloutData> l_callout = std::nullopt;
        PlaceHolder l_placeHolder = PlaceHolder::ASYNC_PEL;

        if (typeid(l_ex) == typeid(EepromException))
        {
            l_placeHolder = PlaceHolder::ASYNC_PEL_WITH_INV_CALLOUT;
            l_callout =
                types::InventoryCalloutData{std::string(SYSTEM_VPD_FILE_PATH),
                                            types::CalloutPriority::High};
        }

        m_logger->logMessage(
            std::format("Exception while performing initial set up. Error: {}",
                        EventLogger::getErrorMsg(l_ex)),
            l_placeHolder,
            types::PelInfoTuple{EventLogger::getErrorType(l_ex),
                                types::SeverityType::Critical, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                l_callout});
    }
}

void IbmHandler::collectionStatusChangeCallback(
    sdbusplus::message_t& i_msg) const noexcept
{
    try
    {
        if (i_msg.is_method_error())
        {
            throw std::runtime_error(
                "Error reading callback message for collection status");
        }

        std::string l_interface;
        types::PropertyMap l_propMap;
        i_msg.read(l_interface, l_propMap);

        const auto l_itr = l_propMap.find("Status");
        if (l_itr == l_propMap.end())
        {
            m_logger->logMessage("Status property missing in property map. "
                                 "Returning without processing.");
            return;
        }

        const auto l_status = std::get_if<std::string>(&(l_itr->second));
        if (l_status == nullptr)
        {
            throw std::runtime_error(
                "Invalid type received in variant for collection status");
        }

        if (vpd::types::CommonProgress::convertOperationStatusFromString(
                *l_status) == types::VpdCollectionStatus::Completed ||
            vpd::types::CommonProgress::convertOperationStatusFromString(
                *l_status) == types::VpdCollectionStatus::Failed)
        {
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
                        m_sysCfgJsonObj["correlatedPropertiesConfigPath"]
                            .get<std::string>());
                }
                else
                {
                    m_logger->logMessage(
                        "Correlated properties JSON path is not defined in system config JSON. Correlated properties listener is disabled.");
                }
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Collection status change callback failed, reason: {}",
                        l_ex.what()));
    }
}

void IbmHandler::updateVpdCollectionStatus(
    const types::VpdCollectionStatus i_status) const noexcept
{
    m_progressInterface->set_property(
        "Status",
        types::CommonProgress::convertOperationStatusToString(i_status));
    m_progressInterface->signal_property("Status");
}

void IbmHandler::addOrRestoreAvailableProperty(
    types::ObjectMap& io_objectInterfaceMap)
{
    try
    {
        for (auto& [l_inventoryPath, l_interfaceMap] : io_objectInterfaceMap)
        {
            auto l_mapperObjectMap = dbusUtility::getObjectMap(
                l_inventoryPath.str, {constants::availabilityInf});

            // If property exists under PIM, skip this inventory path
            auto l_it =
                std::find_if(l_mapperObjectMap.begin(), l_mapperObjectMap.end(),
                             [](const auto& pair) {
                                 return pair.first == constants::pimServiceName;
                             });
            if (l_it != l_mapperObjectMap.end())
            {
                // The object is already under PIM. No need to process
                // again. Retain the old value
                continue;
            }

            // Property doesn't exist on D-Bus. Populate it with default
            // value "false".
            types::PropertyMap l_availableProperty;
            l_availableProperty.emplace(constants::availableProperty, false);
            l_interfaceMap.emplace(constants::availabilityInf,
                                   std::move(l_availableProperty));
        }
    }
    catch (const std::exception& l_ex)
    {
        // TODO: Can we have "&" option in logMessage API in case we want to
        // log at multiple places?
        m_logger->logMessage(std::format(
            "Exception caught while updating Available property. Error {}",
            EventLogger::getErrorMsg(l_ex)));
    }
}

void IbmHandler::resetNonSystemInvPaths(
    types::ObjectMap& io_objectMap) const noexcept
{
    try
    {
        // For all inventory paths other than system inventory path:
        // 1. Preserve the existing interfaces and properties in the map
        // 2. Reset the existing properties in the map to default values
        for (auto& [l_path, l_interfaceMap] : io_objectMap)
        {
            // Skip the system inventory path - keep it unchanged
            if (l_path == sdbusplus::object_path(constants::systemInvPath))
            {
                continue;
            }

            uint16_t l_errCode = 0;
            types::InterfaceMap l_resetInterfaceMap;

            vpdSpecificUtility::resetDataUnderPIM(
                l_path.str, l_resetInterfaceMap, true, l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(std::format(
                    "Failed to reset data for path [{}], error: {}. Skipping.",
                    l_path.str, commonUtility::getErrCodeMsg(l_errCode)));
                continue;
            }

            // Replace the interface map with reset data
            l_interfaceMap = std::move(l_resetInterfaceMap);
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Error while filtering system VPD map for non system inventory path: {}",
            l_ex.what()));
    }
}

} // namespace vpd
