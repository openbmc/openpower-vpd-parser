#include "event_logger.hpp"

#include "logger.hpp"

#include <systemd/sd-bus.h>

namespace vpd
{
const std::unordered_map<types::SeverityType, std::string>
    EventLogger::m_severityMap = {
        {types::SeverityType::Notice,
         "xyz.openbmc_project.Logging.Entry.Level.Notice"},
        {types::SeverityType::Informational,
         "xyz.openbmc_project.Logging.Entry.Level.Informational"},
        {types::SeverityType::Debug,
         "xyz.openbmc_project.Logging.Entry.Level.Debug"},
        {types::SeverityType::Warning,
         "xyz.openbmc_project.Logging.Entry.Level.Warning"},
        {types::SeverityType::Critical,
         "xyz.openbmc_project.Logging.Entry.Level.Critical"},
        {types::SeverityType::Emergency,
         "xyz.openbmc_project.Logging.Entry.Level.Emergency"},
        {types::SeverityType::Alert,
         "xyz.openbmc_project.Logging.Entry.Level.Alert"},
        {types::SeverityType::Error,
         "xyz.openbmc_project.Logging.Entry.Level.Error"}};

const std::unordered_map<types::ErrorType, std::string>
    EventLogger::m_errorMsgMap = {
        {types::ErrorType::DefaultValue, "com.ibm.VPD.Error.DefaultValue"},
        {types::ErrorType::InvalidVpdMessage, "com.ibm.VPD.Error.InvalidVPD"},
        {types::ErrorType::VpdMismatch, "com.ibm.VPD.Error.Mismatch"},
        {types::ErrorType::InvalidEeprom,
         "com.ibm.VPD.Error.InvalidEepromPath"},
        {types::ErrorType::EccCheckFailed, "com.ibm.VPD.Error.EccCheckFailed"},
        {types::ErrorType::JsonFailure, "com.ibm.VPD.Error.InvalidJson"},
        {types::ErrorType::DbusFailure, "com.ibm.VPD.Error.DbusFailure"},
        {types::ErrorType::InvalidSystem,
         "com.ibm.VPD.Error.UnknownSystemType"},
        {types::ErrorType::EssentialFru,
         "com.ibm.VPD.Error.RequiredFRUMissing"},
        {types::ErrorType::GpioError, "com.ibm.VPD.Error.GPIOError"}};

const std::unordered_map<types::CalloutPriority, std::string>
    EventLogger::m_priorityMap = {{types::CalloutPriority::High, "H"},
                                  {types::CalloutPriority::Medium, "M"},
                                  {types::CalloutPriority::MediumGroupA, "A"},
                                  {types::CalloutPriority::MediumGroupB, "B"},
                                  {types::CalloutPriority::MediumGroupC, "C"},
                                  {types::CalloutPriority::Low, "L"}};

void EventLogger::createAsyncPelWithInventoryCallout(
    const types::ErrorType& i_errorType, const types::SeverityType& i_severity,
    const std::vector<types::InventoryCalloutData>& i_callouts,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc, const std::string& i_description,
    const std::optional<std::string> i_userData1,
    const std::optional<std::string> i_userData2,
    const std::optional<std::string> i_symFru,
    const std::optional<std::string> i_procedure)
{
    (void)i_symFru;
    (void)i_procedure;

    try
    {
        if (i_callouts.empty())
        {
            logging::logMessage("Callout information is missing to create PEL");
            // TODO: Revisit this instead of simpley returning.
            return;
        }

        if (m_errorMsgMap.find(i_errorType) == m_errorMsgMap.end())
        {
            throw std::runtime_error(
                "Error type not found in the error message map to create PEL");
            // TODO: Need to handle, instead of throwing exception. Create
            // default message in message_registry.json.
        }

        const std::string& l_message = m_errorMsgMap.at(i_errorType);

        const std::string& l_severity =
            (m_severityMap.find(i_severity) != m_severityMap.end()
                 ? m_severityMap.at(i_severity)
                 : m_severityMap.at(types::SeverityType::Informational));

        std::string l_description =
            (!i_description.empty() ? i_description : "VPD generic error");

        std::string l_userData1 = (i_userData1) ? (*i_userData1) : "";

        std::string l_userData2 = (i_userData2) ? (*i_userData2) : "";

        const types::InventoryCalloutData& l_invCallout = i_callouts[0];
        // TODO: Need to handle multiple inventory path callout's, when multiple
        // callout's is supported by "Logging" service.

        const types::CalloutPriority& l_priorityEnum = get<1>(l_invCallout);

        const std::string& l_priority =
            (m_priorityMap.find(l_priorityEnum) != m_priorityMap.end()
                 ? m_priorityMap.at(l_priorityEnum)
                 : m_priorityMap.at(types::CalloutPriority::Low));

        sd_bus* l_sdBus = nullptr;
        sd_bus_default(&l_sdBus);

        const uint8_t l_additionalDataCount = 8;
        auto l_rc = sd_bus_call_method_async(
            l_sdBus, NULL, constants::eventLoggingServiceName,
            constants::eventLoggingObjectPath, constants::eventLoggingInterface,
            "Create", NULL, NULL, "ssa{ss}", l_message.c_str(),
            l_severity.c_str(), l_additionalDataCount, "FileName",
            i_fileName.c_str(), "FunctionName", i_funcName.c_str(),
            "InternalRc", std::to_string(i_internalRc).c_str(), "DESCRIPTION",
            l_description.c_str(), "UserData1", l_userData1.c_str(),
            "UserData2", l_userData2.c_str(), "CALLOUT_INVENTORY_PATH",
            get<0>(l_invCallout).c_str(), "CALLOUT_PRIORITY",
            l_priority.c_str());

        if (l_rc < 0)
        {
            logging::logMessage(
                "Error calling sd_bus_call_method_async, Message = " +
                std::string(strerror(-l_rc)));
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Create PEL failed with error: " +
                            std::string(l_ex.what()));
    }
}

void EventLogger::createAsyncPelWithI2cDeviceCallout(
    const types::ErrorType i_errorType, const types::SeverityType i_severity,
    const std::vector<types::DeviceCalloutData>& i_callouts,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc,
    const std::optional<std::pair<std::string, std::string>> i_userData1,
    const std::optional<std::pair<std::string, std::string>> i_userData2)
{
    // TODO, implementation needs to be added.
    (void)i_errorType;
    (void)i_severity;
    (void)i_callouts;
    (void)i_fileName;
    (void)i_funcName;
    (void)i_internalRc;
    (void)i_userData1;
    (void)i_userData2;
}

void EventLogger::createAsyncPelWithI2cBusCallout(
    const types::ErrorType i_errorType, const types::SeverityType i_severity,
    const std::vector<types::I2cBusCalloutData>& i_callouts,
    const std::string& i_fileName, const std::string& i_funcName,
    const uint8_t i_internalRc,
    const std::optional<std::pair<std::string, std::string>> i_userData1,
    const std::optional<std::pair<std::string, std::string>> i_userData2)
{
    // TODO, implementation needs to be added.
    (void)i_errorType;
    (void)i_severity;
    (void)i_callouts;
    (void)i_fileName;
    (void)i_funcName;
    (void)i_internalRc;
    (void)i_userData1;
    (void)i_userData2;
}

void EventLogger::createAsyncPel(const types::ErrorType& i_errorType,
                                 const types::SeverityType& i_severity,
                                 const std::string& i_fileName,
                                 const std::string& i_funcName,
                                 const uint8_t i_internalRc,
                                 const std::string& i_description,
                                 const std::optional<std::string> i_userData1,
                                 const std::optional<std::string> i_userData2,
                                 const std::optional<std::string> i_symFru,
                                 const std::optional<std::string> i_procedure)
{
    (void)i_symFru;
    (void)i_procedure;
    try
    {
        if (m_errorMsgMap.find(i_errorType) == m_errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
            // TODO: Need to handle, instead of throwing an exception.
        }

        const std::string& l_message = m_errorMsgMap.at(i_errorType);

        const std::string& l_severity =
            (m_severityMap.find(i_severity) != m_severityMap.end()
                 ? m_severityMap.at(i_severity)
                 : m_severityMap.at(types::SeverityType::Informational));

        const std::string l_description =
            ((!i_description.empty() ? i_description : "VPD generic error"));

        const std::string l_userData1 = ((i_userData1) ? (*i_userData1) : "");

        const std::string l_userData2 = ((i_userData2) ? (*i_userData2) : "");

        sd_bus* l_sdBus = nullptr;
        sd_bus_default(&l_sdBus);

        // VALUE_6 represents the additional data pair count passing to create
        // PEL. If there any change in additional data, we need to pass the
        // correct number.
        auto l_rc = sd_bus_call_method_async(
            l_sdBus, NULL, constants::eventLoggingServiceName,
            constants::eventLoggingObjectPath, constants::eventLoggingInterface,
            "Create", NULL, NULL, "ssa{ss}", l_message.c_str(),
            l_severity.c_str(), constants::VALUE_6, "FileName",
            i_fileName.c_str(), "FunctionName", i_funcName.c_str(),
            "InternalRc", std::to_string(i_internalRc).c_str(), "DESCRIPTION",
            l_description.c_str(), "UserData1", l_userData1.c_str(),
            "UserData2", l_userData2.c_str());

        if (l_rc < 0)
        {
            logging::logMessage(
                "Error calling sd_bus_call_method_async, Message = " +
                std::string(strerror(-l_rc)));
        }
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage("Async PEL creation failed with an error: " +
                            std::string(l_ex.what()));
    }
}

void EventLogger::createSyncPel(const types::ErrorType& i_errorType,
                                const types::SeverityType& i_severity,
                                const std::string& i_fileName,
                                const std::string& i_funcName,
                                const uint8_t i_internalRc,
                                const std::string& i_description,
                                const std::optional<std::string> i_userData1,
                                const std::optional<std::string> i_userData2,
                                const std::optional<std::string> i_symFru,
                                const std::optional<std::string> i_procedure)
{
    (void)i_symFru;
    (void)i_procedure;
    try
    {
        if (m_errorMsgMap.find(i_errorType) == m_errorMsgMap.end())
        {
            throw std::runtime_error("Unsupported error type received");
            // TODO: Need to handle, instead of throwing an exception.
        }

        const std::string& l_message = m_errorMsgMap.at(i_errorType);

        const std::string& l_severity =
            (m_severityMap.find(i_severity) != m_severityMap.end()
                 ? m_severityMap.at(i_severity)
                 : m_severityMap.at(types::SeverityType::Informational));

        const std::string l_description =
            ((!i_description.empty() ? i_description : "VPD generic error"));

        const std::string l_userData1 = ((i_userData1) ? (*i_userData1) : "");

        const std::string l_userData2 = ((i_userData2) ? (*i_userData2) : "");

        std::map<std::string, std::string> l_additionalData{
            {"FileName", i_fileName},
            {"FunctionName", i_funcName},
            {"DESCRIPTION", l_description},
            {"InteranlRc", std::to_string(i_internalRc)},
            {"UserData1", l_userData1.c_str()},
            {"UserData2", l_userData2.c_str()}};

        auto l_bus = sdbusplus::bus::new_default();
        auto l_method =
            l_bus.new_method_call(constants::eventLoggingServiceName,
                                  constants::eventLoggingObjectPath,
                                  constants::eventLoggingInterface, "Create");
        l_method.append(l_message, l_severity, l_additionalData);
        l_bus.call(l_method);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage("Sync PEL creation failed with an error: " +
                            std::string(l_ex.what()));
    }
}
} // namespace vpd
