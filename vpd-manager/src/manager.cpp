#include "config.h"

#include "manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "parser.hpp"
#include "parser_factory.hpp"
#include "parser_interface.hpp"
#include "single_fab.hpp"
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
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& progressiFace,
    const std::shared_ptr<sdbusplus::asio::connection>& asioConnection) :
    m_ioContext(ioCon), m_interface(iFace), m_progressInterface(progressiFace),
    m_asioConnection(asioConnection), m_logger(Logger::getLoggerInstance())
{
#ifdef IBM_SYSTEM_SINGLE_FAB
    if (!dbusUtility::isChassisPowerOn())
    {
        SingleFab l_singleFab;
        const int& l_rc = l_singleFab.singleFabImOverride();

        if (l_rc == constants::FAILURE)
        {
            throw std::runtime_error(
                std::string(__FUNCTION__) +
                " : Found an invalid system configuration. Needs manual intervention. BMC is being quiesced.");
        }
    }
#endif

    try
    {
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

        iFace->register_method("CollectAllFRUVPD", [this]() -> bool {
            return this->collectAllFruVpd();
        });

        // Indicates FRU VPD collection for the system has not started.
        progressiFace->register_property_rw<std::string>(
            "Status", sdbusplus::vtable::property_::emits_change,
            [this](const std::string& l_currStatus, const auto&) {
                if (m_vpdCollectionStatus != l_currStatus)
                {
                    m_vpdCollectionStatus = l_currStatus;
                    m_interface->signal_property("Status");
                }
                return true;
            },
            [this](const auto&) { return m_vpdCollectionStatus; });

        uint16_t l_errCode = 0;

        if (!commonUtility::isFieldModeEnabled(l_errCode))
        {
            if (l_errCode)
            {
                m_logger->logMessage(
                    "Default mode set. Error while trying to check if field mode is enabled, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }
            else
            {
                m_vpdCollectionMode =
                    commonUtility::getVpdCollectionMode(l_errCode);

                if (l_errCode)
                {
                    m_logger->logMessage(
                        "Default mode set. Error while trying to read VPD collection mode: " +
                        commonUtility::getErrCodeMsg(l_errCode));
                }
            }
        }

        // If required, instantiate OEM specific handler here.
#ifdef IBM_SYSTEM
        m_ibmHandler = std::make_shared<IbmHandler>(
            m_worker, m_backupAndRestoreObj, m_interface, m_progressInterface,
            m_ioContext, m_asioConnection);
#else
        m_worker = std::make_shared<Worker>(INVENTORY_JSON_DEFAULT);
        m_progressInterface->set_property(
            "Status", std::string(constants::vpdCollectionCompleted));
#endif
    }
    catch (const std::exception& e)
    {
        logging::logMessage(
            "Manager class instantiation failed. " + std::string(e.what()));

        vpd::EventLogger::createSyncPel(
            vpd::EventLogger::getErrorType(e), vpd::types::SeverityType::Error,
            __FILE__, __FUNCTION__, 0, vpd::EventLogger::getErrorMsg(e),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    }
}

int Manager::updateKeyword(const types::Path i_vpdPath,
                           const types::WriteVpdParams i_paramsToWriteData)
{
    if (i_vpdPath.empty())
    {
        logging::logMessage("Given VPD path is empty.");
        return -1;
    }

    uint16_t l_errCode = 0;
    types::Path l_fruPath;
    nlohmann::json l_sysCfgJsonObj{};

    if (m_worker.get() != nullptr)
    {
        l_sysCfgJsonObj = m_worker->getSysCfgJsonObj();

        // Get the EEPROM path
        if (!l_sysCfgJsonObj.empty())
        {
            l_fruPath = jsonUtility::getFruPathFromJson(l_sysCfgJsonObj,
                                                        i_vpdPath, l_errCode);
        }
    }

    if (l_fruPath.empty())
    {
        if (l_errCode)
        {
            logging::logMessage(
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
                logging::logMessage(
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

        // update keyword in inherited FRUs
        if (l_rc != constants::FAILURE)
        {
            vpdSpecificUtility::updateKwdOnInheritedFrus(
                l_fruPath, l_writeParams, l_sysCfgJsonObj, l_errCode);

            if (l_errCode)
            {
                logging::logMessage(
                    "Failed to update keyword on inherited FRUs for FRU [" +
                    l_fruPath +
                    "] , error : " + commonUtility::getErrCodeMsg(l_errCode));
            }
        }

        // log VPD write success or failure
        auto l_logger = Logger::getLoggerInstance();

        // update common interface(s) properties
        if (l_rc != constants::FAILURE)
        {
            vpdSpecificUtility::updateCiPropertyOfInheritedFrus(
                l_fruPath, l_writeParams, l_sysCfgJsonObj, l_errCode);

            if (l_errCode)
            {
                l_logger->logMessage(
                    "Failed to update Ci property of inherited FRUs, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }
        }

        l_logger->logMessage(
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

        std::shared_ptr<Parser> l_parserObj = std::make_shared<Parser>(
            i_fruPath, l_sysCfgJsonObj, m_vpdCollectionMode);
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

        std::shared_ptr<vpd::Parser> l_parserObj =
            std::make_shared<vpd::Parser>(i_fruPath, l_jsonObj,
                                          m_vpdCollectionMode);

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
    if (m_vpdCollectionStatus != constants::vpdCollectionCompleted)
    {
        logging::logMessage(
            "Currently VPD CollectionStatus is not completed. Cannot perform single FRU VPD collection for " +
            std::string(i_dbusObjPath));
        return;
    }

    if (m_worker.get() != nullptr)
    {
        m_worker->collectSingleFruVpd(i_dbusObjPath);
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

void Manager::performVpdRecollection()
{
    if (m_worker.get() != nullptr)
    {
        m_worker->performVpdRecollection();
    }
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

        EventLogger::createSyncPel(
            types::ErrorType::FirmwareError, l_severityType, __FILE__,
            __FUNCTION__, 0, "Collect all FRUs VPD is requested.", std::nullopt,
            std::nullopt, std::nullopt, std::nullopt);

// ToDo: Handle with OEM interface
#ifdef IBM_SYSTEM
        if (m_ibmHandler.get() != nullptr)
        {
            m_ibmHandler->collectAllFruVpd();
            return true;
        }
        else
        {
            throw std::runtime_error(
                "Not found any OEM handler to collect all FRUs VPD.");
        }
#endif
    }
    catch (const std::exception& l_ex)
    {
        EventLogger::createSyncPel(
            EventLogger::getErrorType(l_ex), types::SeverityType::Warning,
            __FILE__, __FUNCTION__, 0, std::string(l_ex.what()), std::nullopt,
            std::nullopt, std::nullopt, std::nullopt);
    }
    return false;
}
} // namespace vpd
