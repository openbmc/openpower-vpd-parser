#include "thread_manager.hpp"

#include "logger.hpp"
#include "worker.hpp"

#include <format>
#include <thread>

namespace vpd
{

ThreadManager::ThreadManager(
    const std::shared_ptr<Worker>& i_worker,
    const std::shared_ptr<ConfigManager>& i_configManager) :
    m_configManager(i_configManager), m_worker(i_worker),
    m_logger(Logger::getLoggerInstance())
{}

void ThreadManager::collectAllChassisVpd(
    const std::map<std::string, std::string>& i_chassisToEepromMap)
{
    if (i_chassisToEepromMap.empty() || m_chassisIdToJsonMap.empty())
    {
        m_logger->logMessage(
            "Either chassisToEeprom map or chassisIdToJson map is empty. Nothing to collect.");
        return;
    }
    m_logger->logMessage(
        std::format(
            "Starting multi-threaded motherboard VPD collection for {} chassis",
            i_chassisToEepromMap.size()),
        PlaceHolder::COLLECTION);

    // Spawn a thread for each chassis motherboard
    for (const auto& [l_chassisId, l_eepromPath] : i_chassisToEepromMap)
    {
        // Get the chassis-specific JSON
        auto l_jsonItr = m_chassisIdToJsonMap.find(l_chassisId);
        if (l_jsonItr == m_chassisIdToJsonMap.end())
        {
            m_logger->logMessage(std::format(
                "Chassis ID [{}] not found in chassis ID to JSON map. Skipping motherboard VPD collection.",
                l_chassisId));
            continue;
        }

        const nlohmann::json& l_chassisJson = l_jsonItr->second;

        m_logger->logMessage(
            std::format("Spawning thread for chassis [{}] with EEPROM path [{}]",
                        l_chassisId, l_eepromPath),
            PlaceHolder::COLLECTION);

        try
        {
            std::thread{[l_eepromPath, l_chassisJson, this]() {
                // Create a local Worker instance for this thread
                Worker l_threadWorker;

                uint16_t l_errCode = 0;
                auto [l_isCollectionSuccessful, l_collectionStatus] =
                    l_threadWorker.processFruCollection(
                        l_eepromPath, l_chassisJson, l_errCode);

                m_logger->logMessage(std::format(
                    "Thread [{}] completed VPD collection for EEPROM [{}]. "
                    "Success: {}, Status: {}, ErrorCode: {}",
                    std::this_thread::get_id(), l_eepromPath,
                    l_isCollectionSuccessful, l_collectionStatus, l_errCode),PlaceHolder::COLLECTION);
            }}.detach();
            //ToDo:- this detach mode is for time being, we need to update the system view post collection.
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
    if (!m_configManager)
    {
        std::cout
            << "ERROR: ConfigManager is null, cannot proceed with VPD collection"
            << std::endl;
        return;
    }

    // Get the chassis to motherboard EEPROM path map from ConfigManager
    const auto& l_chassisToMotherboardEepromMap =
        m_configManager->getChassisToMotherboardEepromMap();

    // Get the chassisId to json map for chassis-specific configuration
    m_chassisIdToJsonMap = m_configManager->getChassisIdToJsonMap();

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
    collectAllChassisVpd(l_chassisToMotherboardEepromMap);
}
} // namespace vpd
