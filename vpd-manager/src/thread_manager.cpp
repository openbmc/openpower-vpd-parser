#include "thread_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "utility/event_logger_utility.hpp"
#include "utility/vpd_specific_utility.hpp"
#include "worker.hpp"

#include <format>
#include <thread>

namespace vpd
{

ThreadManager::ThreadManager(
    const std::shared_ptr<ConfigManager>& i_configManager,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>&
        i_progressInterface) :
    m_configManager(i_configManager), m_progressInterface(i_progressInterface),
    m_logger(Logger::getLoggerInstance())
{
    if (!m_configManager)
    {
        throw std::invalid_argument(
            "ConfigManager cannot be null - it is mandatory for ThreadManager instantiation");
    }

    if (!m_progressInterface)
    {
        throw std::invalid_argument(
            "Progress interface can not be null, it is mandatory for ThreadManager instantiation");
    }
}

void ThreadManager::updateOverallCollectionStatus(
    const types::VpdCollectionStatus i_status) const noexcept
{
    m_progressInterface->set_property(
        "Status",
        types::CommonProgress::convertOperationStatusToString(i_status));
    m_progressInterface->signal_property("Status");
}

void ThreadManager::collectAllChassisVpd()
{
    // Get the chassis to motherboard EEPROM path map from ConfigManager
    const auto& l_chassisToMotherboardEepromMap =
        m_configManager->getChassisToMotherboardEepromMap();

    // Get the chassisId to json map for chassis-specific configuration
    const auto& l_chassisIdToJsonMap = m_configManager->getChassisIdToJsonMap();

    if (!(l_chassisToMotherboardEepromMap.empty()) || l_chassisIdToJsonMap.empty())
    {
        std::string l_errorMsg =
            "chassisToEeprom map or chassisIdToJson map is empty. "
            "VPD collection cannot proceed due to missing system configuration.";
        throw JsonException(l_errorMsg);
    }

    m_logger->logMessage(
        std::format(
            "Starting multi-threaded motherboard VPD collection for {} chassis",
            l_chassisToMotherboardEepromMap.size()),
        PlaceHolder::COLLECTION);

    for (const auto& [l_chassisId, l_eepromPath] :
         l_chassisToMotherboardEepromMap)
    {
        // TODO: Add check to see if the eeprom path is SYSTEM_VPD_FILE_PATH and
        // handle it accordingly.

        auto l_chassisToJsonItr = l_chassisIdToJsonMap.find(l_chassisId);
        if (l_chassisToJsonItr == l_chassisIdToJsonMap.end())
        {
            m_logger->logMessage(std::format(
                "{} not found in chassis ID to JSON map. Skipping motherboard VPD collection.",
                l_chassisId));

            // Update per-chassis collection status to Failed
            uint16_t l_errCode = 0;
            vpdSpecificUtility::setCollectionStatusProperty(
                l_eepromPath, types::VpdCollectionStatus::Failed,
                m_configManager->getJsonObj(), l_errCode);

            if (l_errCode)
            {
                m_logger->logMessage(std::format(
                    "Failed to update collection status for EEPROM [{}], error code: {}",
                    l_eepromPath, l_errCode));
            }
            continue;
        }

        m_logger->logMessage(
            std::format(
                "Spawning thread for chassis [{}] with EEPROM path [{}]",
                l_chassisId, l_eepromPath),
            PlaceHolder::COLLECTION);

        try
        {
            const nlohmann::json& l_chassisJson = l_chassisToJsonItr->second;

            std::thread{[l_eepromPath, l_chassisJson, this]() {
                // Create a local Worker instance for this thread
                Worker l_threadWorker;

                uint16_t l_errCode = 0;
                auto [l_isPresent, l_collectionStatus] =
                    l_threadWorker.collectFruVpd(l_eepromPath, l_chassisJson,
                                                 l_errCode);

                // If collectFruVpd returned with an error code, Update
                // collection status to Failed in such cases.
                if (l_errCode != 0)
                {
                    uint16_t l_statusErrCode = 0;
                    vpdSpecificUtility::setCollectionStatusProperty(
                        l_eepromPath, types::VpdCollectionStatus::Failed,
                        l_chassisJson, l_statusErrCode);

                    if (l_statusErrCode)
                    {
                        m_logger->logMessage(std::format(
                            "Failed to update collection status for EEPROM [{}], "
                            "error code: {}",
                            l_eepromPath, l_statusErrCode));
                    }
                }

                m_logger->logMessage(
                    std::format("Completed VPD collection for EEPROM [{}]. "
                                "Present: {}, Status: {}, ErrorCode: {}",
                                l_eepromPath, l_isPresent, l_collectionStatus,
                                l_errCode),
                    PlaceHolder::COLLECTION);
            }}.detach();
            // ToDo:- this detach mode is for time being, we need to update the
            // system view post collection.
        }
        catch (const std::exception& l_ex)
        {
            m_logger->logMessage(std::format(
                "Failed to spawn thread for chassis [{}], EEPROM [{}]. "
                "Error: {}, Type: {}",
                l_chassisId, l_eepromPath, l_ex.what(), typeid(l_ex).name()));
        }
    }
}

void ThreadManager::callAllFruVpd()
{
    /*
     * TODO: Implement multi-threaded VPD collection for all FRUs in the system
     *
     * This API orchestrates VPD collection for all FRUs across all chassis.
     *
     * Implementation plan:
     *
     * 1. Create thread pool for parallel VPD collection:
     * 2. Track progress and publish status to D-Bus:
     *    a. Update collection status (In Progress, Completed, Failed)
     * 3. Handle errors gracefully:
     *
     */

    /* `updateOverallCollectionStatus` api will be reordered based on the future
     * implementation.*/

    updateOverallCollectionStatus(types::VpdCollectionStatus::InProgress);

    try
    {
        collectAllChassisVpd();

        // TODO: This is temporary - Will be updated once proper
        // thread pool with completion tracking is implemented.
        updateOverallCollectionStatus(types::VpdCollectionStatus::Completed);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            std::format("VPD collection failed with exception: {}, Type: {}",
                        l_ex.what(), typeid(l_ex).name()),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::InternalFailure,
                                types::SeverityType::Critical, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});

        updateOverallCollectionStatus(types::VpdCollectionStatus::Failed);
    }
}
} // namespace vpd
