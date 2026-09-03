#include "config.h"

#include "manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "gpio_monitor.hpp"
#include "parser.hpp"
#include "parser_factory.hpp"
#include "parser_interface.hpp"
#include "types.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/json_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <boost/asio/steady_timer.hpp>
#include <com/ibm/VPD/error.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

#include <cerrno>
#include <cstring>
#include <format>
#include <fstream>

namespace vpd
{
Manager::Manager(
    const std::shared_ptr<boost::asio::io_context>& ioCon,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& iFace,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& progressiFace,
    const std::shared_ptr<sdbusplus::asio::connection>& asioConnection) :
    m_ioContext(ioCon), m_interface(iFace), m_progressInterface(progressiFace),
    m_asioConnection(asioConnection), m_logger(Logger::getLoggerInstance())
{
    m_logger->setConn(m_asioConnection);

    try
    {
        // For backward compatibility. Should be deprecated.
        iFace->register_method(
            "WriteKeyword",
            [this](const sdbusplus::object_path i_path,
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
            [this](const sdbusplus::object_path& i_dbusObjPath) {
                this->collectSingleFruVpd(i_dbusObjPath);
            });

        iFace->register_method(
            "DeleteFRUVPD",
            [this](const sdbusplus::object_path& i_dbusObjPath) {
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
            [this](const sdbusplus::object_path& i_dbusObjPath)
                -> types::EepromPathList {
                return this->getHwPath(i_dbusObjPath);
            });

        iFace->register_method("PerformVPDRecollection", [this]() {
            this->performVpdRecollection();
        });

        iFace->register_method("CollectAllFRUVPD", [this]() -> bool {
            return this->collectAllFruVpd();
        });

        iFace->register_method("DeleteAllFRUVPD", [this]() {
            this->deleteAllFRUVPD();
        });

        iFace->register_method(
            "ValidateRedundantEEPROM",
            [this](const types::Path& i_fruPath) -> bool {
                return this->validateRedundantEeprom(i_fruPath);
            });

        iFace->register_method(
            "GetParsedVPD",
            [this](const types::Path& i_fruPath) -> types::ParsedVpdMap {
                return this->getParsedVpd(i_fruPath);
            });

        // Indicates FRU VPD collection for the system has not started.
        progressiFace->register_property_rw<std::string>(
            "Status", sdbusplus::vtable::property_::emits_change,
            [this](const std::string& l_currStatus, const auto&) {
                m_vpdCollectionStatus = l_currStatus;
                return true;
            },
            [this](const auto&) { return m_vpdCollectionStatus; });

        ConfigManager::ManagerPassKey l_configMgrKey;

        // initialize ConfigManager with the default JSON so that any
        // JSON utility APIs invoked during system VPD collection (before the
        // correct symlink is known) have a valid singleton instance.
        m_configManager =
            ConfigManager::initialize(l_configMgrKey, INVENTORY_JSON_DEFAULT);

        // If required, instantiate OEM specific handler here.
#ifdef IBM_SYSTEM
        readVpdCollectionMode();

        m_ibmHandler = std::make_shared<IbmHandler>(
            m_backupAndRestoreObj, m_interface, m_progressInterface,
            m_ioContext, m_asioConnection, m_vpdCollectionMode);

        // once IBM handler is initialized, the symlink points to the
        // correct system-specific JSON. Call initialize() again to build
        // configuration maps with the correct system-specific configuration
        // JSON
        m_configManager =
            ConfigManager::initialize(l_configMgrKey, INVENTORY_JSON_SYM_LINK);
#else
        m_progressInterface->set_property(
            "Status", std::string(constants::vpdCollectionCompleted));
#endif

#ifdef IBM_SYSTEM

        // Instantiate Listener object after ConfigManager is initialized
        m_eventListener =
            std::make_shared<Listener>(m_configManager, m_asioConnection);

        m_ibmHandler->initIbmListenerObject(m_eventListener);
#endif

        // Initialize the GpioMonitor
        m_gpioMonitor =
            std::make_shared<GpioMonitor>(m_configManager, m_ioContext);

        // Initialize thread manager
        m_threadManager = std::make_unique<ThreadManager>(m_configManager,
                                                          m_progressInterface);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Manager class instantiation failed. " + std::string(l_ex.what()));

        m_logger->logMessage(
            std::string("Manager class instantiation failed. Reason:") +
                EventLogger::getErrorMsg(l_ex),
            PlaceHolder::PEL,
            types::PelInfoTuple{EventLogger::getErrorType(l_ex),
                                types::SeverityType::Error, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});
    }
}

void Manager::readVpdCollectionMode() noexcept
{
    const auto l_removeLabModeFile = [this]() {
        std::error_code l_ec;
        if (std::filesystem::exists(constants::singleChassisLabModeFile, l_ec))
        {
            std::filesystem::remove(constants::singleChassisLabModeFile, l_ec);
            if (l_ec)
            {
                m_logger->logMessage(
                    "Failed to delete file: " +
                    std::string(constants::singleChassisLabModeFile) +
                    ", error: " + l_ec.message());
            }
        }
        else if (l_ec)
        {
            m_logger->logMessage(
                "Failed to check existence of file: " +
                std::string(constants::singleChassisLabModeFile) +
                ", error: " + l_ec.message());
        }
    };

    uint16_t l_errCode{0};
    // check VPD collection mode
    if (!commonUtility::isFieldModeEnabled(l_errCode))
    {
        if (l_errCode)
        {
            m_logger->logMessage(
                "Default mode set. Error while trying to check if field mode is enabled, error : " +
                commonUtility::getErrCodeMsg(l_errCode));

            l_removeLabModeFile();
            return;
        }

        m_vpdCollectionMode = commonUtility::getVpdCollectionMode(l_errCode);

        if (l_errCode)
        {
            m_logger->logMessage(
                "Default mode set. Error while trying to read VPD collection mode: " +
                commonUtility::getErrCodeMsg(l_errCode));
            l_removeLabModeFile();
            return;
        }

        if (m_vpdCollectionMode == types::VpdCollectionMode::FILE_MODE)
        {
            std::error_code l_ec;
            std::filesystem::create_directories(
                std::filesystem::path(constants::singleChassisLabModeFile)
                    .parent_path(),
                l_ec);

            if (l_ec)
            {
                m_logger->logMessage(
                    "Failed to create parent directory for file: " +
                    std::string(constants::singleChassisLabModeFile) +
                    ", error: " + l_ec.message());
            }
            else
            {
                // We just want the file to exist.
                std::ofstream l_file(constants::singleChassisLabModeFile);
                if (!l_file)
                {
                    m_logger->logMessage(
                        "Failed to create file: " +
                        std::string(constants::singleChassisLabModeFile) +
                        ", error: " + std::string(strerror(errno)));
                }
            }
        }
        else
        {
            l_removeLabModeFile();
        }
    }
}

int Manager::updateKeyword(const types::Path i_vpdPath,
                           const types::WriteVpdParams i_paramsToWriteData)
{
    if (i_vpdPath.empty())
    {
        throw types::DbusInvalidArgument();
    }

    uint16_t l_errCode = 0;
    types::Path l_fruPath;
    nlohmann::json l_sysCfgJsonObj{};

    if (m_configManager)
    {
        const auto l_jsonResult = m_configManager->getJsonObj(i_vpdPath);
        if (l_jsonResult.has_value())
        {
            l_sysCfgJsonObj = l_jsonResult.value().get();
        }
        else
        {
            m_logger->logMessage(std::format(
                "JSON not found for path {}.{}", i_vpdPath,
                commonUtility::getErrCodeMsg(l_jsonResult.error())));
        }

        // Get the EEPROM path
        if (!l_sysCfgJsonObj.empty())
        {
            l_fruPath = jsonUtility::getFruPathFromJson(i_vpdPath, l_errCode);
        }
    }

    if (l_fruPath.empty())
    {
        if (l_errCode)
        {
            m_logger->logMessage(
                "Failed to get FRU path from JSON for [" + i_vpdPath +
                "], error : " + commonUtility::getErrCodeMsg(l_errCode));
        }

        l_fruPath = i_vpdPath;
    }

    try
    {
        std::shared_ptr<Parser> l_parserObj = std::make_shared<Parser>(
            l_fruPath, l_sysCfgJsonObj, m_vpdCollectionMode);

        types::DbusVariantType l_updatedValue;
        auto l_rc =
            l_parserObj->updateVpdKeyword(i_paramsToWriteData, l_updatedValue);

        if (l_rc != constants::FAILURE && m_backupAndRestoreObj)
        {
            if (m_backupAndRestoreObj->updateKeywordOnPrimaryOrBackupPath(
                    l_fruPath, i_paramsToWriteData) < constants::VALUE_0)
            {
                m_logger->logMessage(
                    "Write success, but backup and restore failed for file[" +
                    l_fruPath + "]");
            }
        }

        types::WriteVpdParams l_writeParams;
        types::BinaryVector l_valueToUpdate;

        if (const types::IpzData* l_ipzData =
                std::get_if<types::IpzData>(&i_paramsToWriteData))
        {
            if (const types::BinaryVector* l_val =
                    std::get_if<types::BinaryVector>(&l_updatedValue))
            {
                l_valueToUpdate = *l_val;
            }
            else
            {
                l_valueToUpdate = std::get<2>(*l_ipzData);
            }
            l_writeParams =
                std::make_tuple(std::get<0>(*l_ipzData),
                                std::get<1>(*l_ipzData), l_valueToUpdate);
        }
        else if (const types::KwData* l_kwData =
                     std::get_if<types::KwData>(&i_paramsToWriteData))
        {
            if (const types::BinaryVector* l_val =
                    std::get_if<types::BinaryVector>(&l_updatedValue))
            {
                l_valueToUpdate = *l_val;
            }
            else
            {
                l_valueToUpdate = std::get<1>(*l_kwData);
            }

            l_writeParams =
                std::make_tuple(std::get<0>(*l_kwData), l_valueToUpdate);
        }
        else
        {
            // write parameters are invalid
            throw types::DbusInvalidArgument();
        }

        m_logger->logMessage(
            "VPD write " +
                std::string(
                    (l_rc != constants::FAILURE) ? "successful" : "failed") +
                " on path[" + i_vpdPath + "] : " +
                vpdSpecificUtility::convertWriteVpdParamsToString(l_writeParams,
                                                                  l_errCode),
            PlaceHolder::VPD_WRITE);

        return l_rc;
    }
    catch (const std::exception& l_exception)
    {
        m_logger->logMessage("Update keyword failed for file[" + i_vpdPath +
                             "], reason: " + std::string(l_exception.what()));

        m_logger->logMessage(
            std::string("Update keyword failed for file[") + i_vpdPath +
                "], reason:" + std::string(l_exception.what()),
            PlaceHolder::PEL,
            types::PelInfoTuple{EventLogger::getErrorType(l_exception),
                                types::SeverityType::Error, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});

        return -1;
    }
}

int Manager::updateKeywordOnHardware(
    const types::Path i_fruPath,
    const types::WriteVpdParams i_paramsToWriteData)
{
    if (i_fruPath.empty())
    {
        throw types::DbusInvalidArgument();
    }

    try
    {
        nlohmann::json l_sysCfgJsonObj{};

        if (m_configManager)
        {
            const auto l_jsonResult = m_configManager->getJsonObj(i_fruPath);

            if (l_jsonResult.has_value())
            {
                l_sysCfgJsonObj = l_jsonResult.value().get();
            }
            else
            {
                m_logger->logMessage(std::format(
                    "JSON not found for path {}. {}", i_fruPath,
                    commonUtility::getErrCodeMsg(l_jsonResult.error())));
            }
        }

        std::shared_ptr<Parser> l_parserObj = std::make_shared<Parser>(
            i_fruPath, l_sysCfgJsonObj, m_vpdCollectionMode);
        return l_parserObj->updateVpdKeywordOnHardware(i_paramsToWriteData);
    }
    catch (const std::exception& l_exception)
    {
        m_logger->logMessage(
            std::string("Update keyword on hardware failed for file[") +
                i_fruPath + "], Reason:" + std::string(l_exception.what()),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::InvalidEeprom,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});

        return constants::FAILURE;
    }
}

types::DbusVariantType Manager::readKeyword(
    const types::Path i_fruPath, const types::ReadVpdParams i_paramsToReadData)
{
    if (i_fruPath.empty())
    {
        throw types::DbusInvalidArgument();
    }

    try
    {
        nlohmann::json l_jsonObj{};

        if (m_configManager)
        {
            const auto l_jsonResult = m_configManager->getJsonObj(i_fruPath);
            if (l_jsonResult.has_value())
            {
                l_jsonObj = l_jsonResult.value().get();
            }
            else
            {
                m_logger->logMessage(std::format(
                    "JSON not found for path {}. {}", i_fruPath,
                    commonUtility::getErrCodeMsg(l_jsonResult.error())));
            }
        }

        std::error_code ec;

        // Check if given path is filesystem path
        if (!std::filesystem::exists(i_fruPath, ec) && (ec))
        {
            throw std::runtime_error(
                "Given file path " + i_fruPath + " not found.");
        }

        std::shared_ptr<vpd::Parser> l_parserObj =
            std::make_shared<vpd::Parser>(i_fruPath, l_jsonObj,
                                          m_vpdCollectionMode);

        std::shared_ptr<vpd::ParserInterface> l_vpdParserInstance =
            l_parserObj->getVpdParserInstance();

        return (
            l_vpdParserInstance->readKeywordFromHardware(i_paramsToReadData));
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            l_ex.what() +
            std::string(". VPD manager read operation failed for ") +
            i_fruPath);

        m_logger->logMessage(
            std::string("VPD manager read operation failed for file[") +
                i_fruPath + "], Reason:" + std::string(l_ex.what()),
            PlaceHolder::PEL,
            types::PelInfoTuple{EventLogger::getErrorType(l_ex),
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});

        throw types::DeviceError::ReadFailure();
    }
}

void Manager::collectSingleFruVpd(const sdbusplus::object_path& i_dbusObjPath)
{
    if (std::string(i_dbusObjPath).empty())
    {
        throw types::DbusInvalidArgument();
    }

    if (m_vpdCollectionStatus != constants::vpdCollectionCompleted)
    {
        m_logger->logMessage(
            "Currently VPD CollectionStatus is not completed. Cannot perform single FRU VPD collection for " +
            std::string(i_dbusObjPath));
        return;
    }

    const auto l_configJsonResult =
        m_configManager->getJsonObj(std::string(i_dbusObjPath));

    if (!l_configJsonResult.has_value())
    {
        m_logger->logMessage(std::format(
            "Failed to get JSON for path {}. Error: {}, can't collect VPD.",
            std::string(i_dbusObjPath),
            commonUtility::getErrCodeMsg(l_configJsonResult.error())));
        return;
    }

    Worker{}.collectSingleFruVpd(l_configJsonResult.value().get(),
                                 i_dbusObjPath);
}

void Manager::deleteSingleFruVpd(const sdbusplus::object_path& i_dbusObjPath)
{
    if (std::string(i_dbusObjPath).empty())
    {
        throw types::DbusInvalidArgument();
    }

    try
    {
        if (!m_configManager)
        {
            throw std::runtime_error(std::format(
                "Config manager object not found, can't perform FRU VPD deletion for: {}",
                std::string(i_dbusObjPath)));
        }

        const auto l_configJsonResult =
            m_configManager->getJsonObj(std::string(i_dbusObjPath));

        if (!l_configJsonResult.has_value())
        {
            throw std::runtime_error(std::format(
                "Failed to get JSON for path {}. Error: {}, can't perform FRU VPD deletion.",
                std::string(i_dbusObjPath),
                commonUtility::getErrCodeMsg(l_configJsonResult.error())));
        }

        Worker{}.deleteFruVpd(l_configJsonResult.value().get(),
                              std::string(i_dbusObjPath));
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(l_ex.what());

        m_logger->logMessage(
            std::string("VPD manager delete operation failed for object[") +
                std::string(i_dbusObjPath) +
                "], Reason:" + std::string(l_ex.what()),
            PlaceHolder::PEL,
            types::PelInfoTuple{EventLogger::getErrorType(l_ex),
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
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
    const std::string& i_unexpandedLocationCode, const uint16_t i_nodeNumber)
{
    // Validate the input to prevent integer underflow / bad data
    if (i_nodeNumber > constants::VALUE_12)
    {
        m_logger->logMessage(std::format(
            "Invalid node number received: {}. Maximum supported nodes is 12.",
            i_nodeNumber));
        throw types::DbusInvalidArgument();
    }

    if (!isValidUnexpandedLocationCode(i_unexpandedLocationCode))
    {
        //@todo: a PEL used to be logged here using deprecated
        // phosphor::logging::elog API. Should we still log a PEL here?
        m_logger->logMessage(std::format("Invalid unexpanded location code: {}",
                                         i_unexpandedLocationCode));

        throw types::DbusInvalidArgument();
    }

    if (!m_configManager)
    {
        m_logger->logMessage(std::format(
            "Cannot get expanded location code for {} as ConfigManager instance is not initialized.",
            i_unexpandedLocationCode));

        throw types::DbusInvalidArgument();
    }

    const auto l_nodeLocCodeKeyResult =
        vpdSpecificUtility::buildNodeQualifiedLocCode(i_unexpandedLocationCode,
                                                      i_nodeNumber);

    if (!l_nodeLocCodeKeyResult.has_value())
    {
        throw types::DbusInvalidArgument();
    }

    const auto l_inventoryPathResult =
        m_configManager->getInventoryPath(l_nodeLocCodeKeyResult.value());

    if (!l_inventoryPathResult.has_value())
    {
        m_logger->logMessage(std::format(
            "Failed to get inventory path corresponding to location code {}. Error: {}",
            i_unexpandedLocationCode,
            commonUtility::getErrCodeMsg(l_inventoryPathResult.error())));

        throw types::DbusInvalidArgument();
    }

    auto l_dbusReadRes = dbusUtility::readDbusProperty(
        constants::pimServiceName, l_inventoryPathResult.value(),
        constants::locationCodeInf, "LocationCode");

    const auto l_expandedLocationCodeRes =
        std::get_if<std::string>(&l_dbusReadRes);

    if (l_expandedLocationCodeRes)
    {
        return *l_expandedLocationCodeRes;
    }

    // failed to read expanded location code from D-Bus
    //@todo: a PEL used to be logged here using deprecated
    // phosphor::logging::elog API. Should we still log a PEL here?
    m_logger->logMessage(std::format(
        "Invalid unexpanded location code data type read from D-Bus for {}",
        i_unexpandedLocationCode));

    throw types::DbusInvalidArgument();
}

types::ListOfPaths Manager::getFrusByUnexpandedLocationCode(
    const std::string& i_unexpandedLocationCode, const uint16_t i_nodeNumber)
{
    if (!isValidUnexpandedLocationCode(i_unexpandedLocationCode))
    {
        //@todo: a PEL used to be logged here using deprecated
        // phosphor::logging::elog API. Should we still log a PEL here?
        m_logger->logMessage(std::format("Invalid unexpanded location code: {}",
                                         i_unexpandedLocationCode));

        throw types::DbusInvalidArgument();
    }

    if (!m_configManager)
    {
        m_logger->logMessage(std::format(
            "Cannot get FRUs by unexpanded location code for {} as ConfigManager instance is not initialized",
            i_unexpandedLocationCode));

        throw types::DbusInvalidArgument();
    }

    const auto l_nodeLocCodeKeyResult =
        vpdSpecificUtility::buildNodeQualifiedLocCode(i_unexpandedLocationCode,
                                                      i_nodeNumber);

    if (!l_nodeLocCodeKeyResult.has_value())
    {
        throw types::DbusInvalidArgument();
    }

    const auto l_inventoryPathResult =
        m_configManager->getInventoryPath(l_nodeLocCodeKeyResult.value());

    if (!l_inventoryPathResult.has_value())
    {
        //@todo: a PEL used to be logged here using deprecated
        // phosphor::logging::elog API. Should we still log a PEL here?
        m_logger->logMessage(std::format(
            "Failed to get inventory paths for unexpanded location code: {}. Error: {}",
            i_unexpandedLocationCode,
            commonUtility::getErrCodeMsg(l_inventoryPathResult.error())));

        throw types::DbusInvalidArgument();
    }

    return types::ListOfPaths{l_inventoryPathResult.value()};
}

types::EepromPathList Manager::getHwPath(
    const sdbusplus::object_path& i_dbusObjPath)
{
    const std::string l_dbusObjPathStr = std::string{i_dbusObjPath};

    if (l_dbusObjPathStr.empty())
    {
        throw types::DbusInvalidArgument();
    }

    types::EepromPathList l_result;

    if (!m_configManager)
    {
        //@todo should we throw exception here?
        m_logger->logMessage(std::format(
            "The feature is only supported when system config JSON is initialized. Failed to get hardware path for {}. ConfigManager is not initialized",
            l_dbusObjPathStr));
        return l_result;
    }

    uint16_t l_errCode{constants::VALUE_0};

    // get the primary EEPROM path
    const auto l_primaryEepromPath =
        jsonUtility::getFruPathFromJson(l_dbusObjPathStr, l_errCode);

    if (l_primaryEepromPath.empty())
    {
        m_logger->logMessage(std::format(
            "Failed to get primary EEPROM path for {}. Error: {}",
            l_dbusObjPathStr, commonUtility::getErrCodeMsg(l_errCode)));

        throw sdbusplus::error::com::ibm::vpd::PathNotFound();
    }

    try
    {
        // push the primary path into result vector first
        l_result.emplace_back(l_primaryEepromPath);

        // check if any redundant EEPROM path is there, if yes add it to the
        // result
        const auto l_redundantEepromPath =
            jsonUtility::getRedundantEepromPathFromJson(l_dbusObjPathStr,
                                                        l_errCode);

        if (!l_redundantEepromPath.empty())
        {
            l_result.emplace_back(l_redundantEepromPath);
        }
        else if (l_errCode)
        {
            m_logger->logMessage(std::format(
                "Failed to get redundant EEPROM path for {}. Error: {}",
                l_dbusObjPathStr, commonUtility::getErrCodeMsg(l_errCode)));
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("Failed to get hardware path for {}. Error: {}",
                        l_dbusObjPathStr, l_ex.what()));
    }

    return l_result;
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
    /* @todo - This part is commented as of now , we will have to revisit in
     * future*/
    // std::tuple<std::string, uint16_t> l_locationAndNodePair =
    //     getUnexpandedLocationCode(i_expandedLocationCode);
    // return
    // getFrusByUnexpandedLocationCode(std::get<0>(l_locationAndNodePair),
    //                                       std::get<1>(l_locationAndNodePair));

    // Validate that the location code is expanded.
    if (i_expandedLocationCode.empty() || i_expandedLocationCode[0] != 'U' ||
        i_expandedLocationCode.length() <
            constants::EXP_LOCATION_CODE_MIN_LENGTH)
    {
        m_logger->logMessage(std::format("Invalid expanded location code: {}",
                                         i_expandedLocationCode));
        throw types::DbusInvalidArgument();
    }

    types::ListOfPaths l_fruPaths;

    try
    {
        const auto l_managedObjectsResult = dbusUtility::getManagedObjects(
            constants::pimServiceName, constants::pimPath);

        if (!l_managedObjectsResult)
        {
            if (l_managedObjectsResult.error() != error_code::DBUS_FAILURE)
            {
                m_logger->logMessage(std::format(
                    "Failed to get managed objects for expanded location code {}: Error{}",
                    i_expandedLocationCode,
                    commonUtility::getErrCodeMsg(
                        l_managedObjectsResult.error())));
            }
            return l_fruPaths;
        }

        const auto& l_managedObjects = l_managedObjectsResult.value();

        for (const auto& [l_objPath, l_interfaceMap] : l_managedObjects)
        {
            auto l_locCodeIntfIter =
                l_interfaceMap.find(constants::locationCodeInf);
            if (l_locCodeIntfIter == l_interfaceMap.end())
            {
                continue;
            }

            const auto& l_propertyMap = l_locCodeIntfIter->second;
            auto l_locCodePropIter = l_propertyMap.find("LocationCode");
            if (l_locCodePropIter == l_propertyMap.end())
            {
                continue;
            }

            // Check if the location code matches
            if (const auto l_locCode =
                    std::get_if<std::string>(&l_locCodePropIter->second);
                l_locCode && *l_locCode == i_expandedLocationCode)
            {
                l_fruPaths.emplace_back(l_objPath);
            }
        }

        if (l_fruPaths.empty())
        {
            m_logger->logMessage(
                std::format("No FRUs found for expanded location code: {}",
                            i_expandedLocationCode));
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Unexpected error occurred while getting FRUs for expanded location code {}: Error{}",
            i_expandedLocationCode, l_ex.what()));
    }

    return l_fruPaths;
}

void Manager::performVpdRecollection()
{
    if (!m_configManager)
    {
        m_logger->logMessage(
            "Failed to perform VPD recollection because ConfigManager is not initialized.");
        return;
    }

    m_logger->logMessage(
        "Perform VPD recollection triggered via external D-Bus call.");

    Worker{}.performVpdRecollection(
        m_configManager->getJsonObj().value().get());
}

bool Manager::collectAllFruVpd() const noexcept
{
    try
    {
        types::SeverityType l_severityType;
        if (m_vpdCollectionStatus == constants::vpdCollectionNotStarted)
        {
            l_severityType = types::SeverityType::Informational;
        }
        else if (m_vpdCollectionStatus == constants::vpdCollectionCompleted ||
                 m_vpdCollectionStatus == constants::vpdCollectionFailed)
        {
            l_severityType = types::SeverityType::Warning;
        }
        else
        {
            throw std::runtime_error(
                "Invalid collection status " + m_vpdCollectionStatus +
                ". Aborting all FRUs VPD collection.");
        }

        m_logger->logMessage(
            std::string("Collect all FRUs VPD is requested."),
            PlaceHolder::ASYNC_PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError, l_severityType,
                                0, std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
        // Call ThreadManager API to trigger multi-threaded VPD collection
        // for all FRUs in the system.
        if (m_threadManager.get() != nullptr)
        {
            m_threadManager->collectAllFruVpd();
            return true;
        }
        else
        {
            throw std::runtime_error(
                "ThreadManager is not instantiated for FRU VPD collection");
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::string("Collect all FRUs VPD failed, reason- ") +
                std::string(l_ex.what()),
            PlaceHolder::PEL,
            types::PelInfoTuple{EventLogger::getErrorType(l_ex),
                                types::SeverityType::Warning, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});
    }
    return false;
}

bool Manager::validateRedundantEeprom(const types::Path& i_fruPath) const
{
    bool l_rc{false};

    if (!m_configManager)
    {
        m_logger->logMessage(std::format(
            "Failed to validate redundant Eeprom path for [{}] because ConfigManager is not initialized",
            i_fruPath));
        return l_rc;
    }

    const auto l_jsonObjResult = m_configManager->getJsonObj(i_fruPath);

    if (!l_jsonObjResult.has_value())
    {
        m_logger->logMessage(std::format(
            "Failed to get JSON for path {}. Error: {}, can't validate redundant Eeprom.",
            i_fruPath, commonUtility::getErrCodeMsg(l_jsonObjResult.error())));
        return l_rc;
    }

    const auto& l_jsonObj = l_jsonObjResult.value().get();

    uint16_t l_errCode;
    std::string l_redundantEeprom =
        jsonUtility::getRedundantEepromPathFromJson(i_fruPath, l_errCode);

    if (l_redundantEeprom.empty())
    {
        /* @todo Add support for cases where the input path refers to a
         * redundant EEPROM directly.*/

        phosphor::logging::elog<types::DbusInvalidArgument>(
            types::InvalidArgument::ARGUMENT_NAME("PATH"),
            types::InvalidArgument::ARGUMENT_VALUE(i_fruPath.c_str()));
    }

    try
    {
        Parser l_parserForPrimary(i_fruPath, l_jsonObj, m_vpdCollectionMode);

        return l_parserForPrimary.compareData(l_redundantEeprom);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to validate VPD of [" + i_fruPath +
                "] and its redundant EEPROM [" + l_redundantEeprom +
                "] are identical",
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::InternalFailure,
                                types::SeverityType::Informational, 0,
                                l_ex.what(), std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});
    }
    return l_rc;
}

void Manager::deleteAllFRUVPD() const noexcept
{
    if (m_vpdCollectionStatus == constants::vpdCollectionInProgress)
    {
        m_logger->logMessage(
            "FRU VPD collection is in progress. Cannot perform delete all FRU VPD.");
        return;
    }

    try
    {
        m_logger->logMessage(
            std::string("Delete all FRUs VPD is requested."), PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt, std::nullopt});

        const auto l_inventoryBackupPath{
            constants::pimPrimaryPath /
            std::filesystem::path(constants::pimPath).relative_path()};

        if (!std::filesystem::exists(l_inventoryBackupPath))
        {
            m_logger->logMessage(
                "PIM persist path does not exist. Cannot perform delete all FRU VPD.");
            return;
        }

        bool l_directoryRemoved = false;
        uint16_t l_errCode = 0;

        commonUtility::deleteDirectory(l_inventoryBackupPath,
                                       l_directoryRemoved, l_errCode);

        if (!l_directoryRemoved)
        {
            if (l_errCode)
            {
                m_logger->logMessage(
                    std::format("Failed to delete directory {}, error : {}.",
                                l_inventoryBackupPath.string(),
                                commonUtility::getErrCodeMsg(l_errCode)));
            }
            // Since no directories were removed, skip restarting of service.
            return;
        }

        constexpr auto l_numRetries{constants::VALUE_3};

        for (unsigned l_attempt = 0; l_attempt < l_numRetries; ++l_attempt)
        {
            if (commonUtility::restartService(constants::pimServiceName,
                                              l_errCode))
            {
                // restarting inventory manager service is successful, return
                return;
            }
        }

        if (l_errCode)
        {
            throw std::runtime_error("Failed to restart PIM service, error : " +
                                     commonUtility::getErrCodeMsg(l_errCode));
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to clear inventory backup data from path [" +
                std::string(constants::pimPrimaryPath) +
                "]. Error: " + std::string(l_ex.what()),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::FirmwareError,
                                types::SeverityType::Warning, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});
    }
}

types::ParsedVpdMap Manager::getParsedVpd(const types::Path& i_fruPath)
{
    if (i_fruPath.empty())
    {
        m_logger->logMessage("getParsedVpd: FRU path is empty.",
                             PlaceHolder::DEFAULT);
        throw types::DbusInvalidArgument();
    }

    std::error_code l_ec;
    if (!std::filesystem::exists(i_fruPath, l_ec) || l_ec)
    {
        m_logger->logMessage(
            std::format("getParsedVpd: FRU path does not exist [{}].",
                        i_fruPath),
            PlaceHolder::DEFAULT);
        throw types::DbusInvalidArgument();
    }

    try
    {
        nlohmann::json l_jsonObj{};

        if (m_configManager)
        {
            const auto l_jsonResult = m_configManager->getJsonObj(i_fruPath);
            if (l_jsonResult.has_value())
            {
                l_jsonObj = l_jsonResult.value().get();
            }
            else
            {
                m_logger->logMessage(std::format(
                    "JSON not found for path {}. Error: {}", i_fruPath,
                    commonUtility::getErrCodeMsg(l_jsonResult.error())));
            }
        }

        std::shared_ptr<Parser> l_parserObj =
            std::make_shared<Parser>(i_fruPath, l_jsonObj, m_vpdCollectionMode);

        const types::VPDMapVariant l_parsedVpd = l_parserObj->parse();

        if (const auto* l_ipzMap = std::get_if<types::IPZVpdMap>(&l_parsedVpd))
        {
            // IPZ VPD: convert string values to BinaryVector for D-Bus
            types::DbusIPZVpdMap l_result;
            for (const auto& [l_record, l_kwMap] : *l_ipzMap)
            {
                std::map<types::Keyword, types::BinaryVector> l_kwBinMap;
                for (const auto& [l_keyword, l_value] : l_kwMap)
                {
                    l_kwBinMap.emplace(
                        l_keyword,
                        types::BinaryVector(l_value.begin(), l_value.end()));
                }
                l_result.emplace(l_record, std::move(l_kwBinMap));
            }

            return l_result;
        }

        if (const auto* l_kwMap =
                std::get_if<types::KeywordVpdMap>(&l_parsedVpd))
        {
            // Keyword VPD: BinaryVector and string pass through as-is;
            // only size_t needs casting to uint64_t for D-Bus.
            types::DbusKeywordVpdMap l_result;
            for (const auto& [l_keyword, l_value] : *l_kwMap)
            {
                std::visit(
                    [&](const auto& l_val) {
                        using T = std::decay_t<decltype(l_val)>;
                        if constexpr (std::is_same_v<T, size_t>)
                        {
                            l_result.emplace(l_keyword,
                                             static_cast<uint64_t>(l_val));
                        }
                        else
                        {
                            l_result.emplace(l_keyword, l_val);
                        }
                    },
                    l_value);
            }
            return l_result;
        }

        // std::monostate — unrecognised VPD format
        m_logger->logMessage(
            std::format("getParsedVpd: unrecognised VPD format for FRU [{}].",
                        i_fruPath),
            PlaceHolder::DEFAULT);
        throw types::DbusInvalidArgument();
    }
    catch (const types::DbusInvalidArgument&)
    {
        throw;
    }
    catch (const EccException& l_ex)
    {
        // EccException is thrown by IpzVpdParser when an ECC check fails
        m_logger->logMessage(
            std::format("ECC check failed for FRU [{}], reason: {}", i_fruPath,
                        l_ex.what()),
            PlaceHolder::DEFAULT);

        throw types::DeviceError::ReadFailure();
    }
    catch (const DataException& l_ex)
    {
        // DataException is thrown for malformed or invalid VPD data
        m_logger->logMessage(
            std::format("Invalid VPD data for FRU [{}], reason: {}", i_fruPath,
                        l_ex.what()),
            PlaceHolder::DEFAULT);

        throw types::DeviceError::ReadFailure();
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("getParsedVpd failed for FRU [{}], reason: {}",
                        i_fruPath, l_ex.what()),
            PlaceHolder::DEFAULT);

        throw types::DbusInvalidArgument();
    }
}

} // namespace vpd
