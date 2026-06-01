#include "thread_manager.hpp"

#include "exceptions.hpp"
#include "logger.hpp"
#include "worker.hpp"

#include <utility/json_utility.hpp>

#include <format>
#include <thread>

namespace vpd
{

ThreadManager::ThreadManager(
    const std::shared_ptr<ConfigManager>& i_configManager) :
    m_configManager(i_configManager), m_logger(Logger::getLoggerInstance())
{
    if (!m_configManager)
    {
        throw std::invalid_argument(
            "ConfigManager cannot be null - it is mandatory for ThreadManager instantiation");
    }
}

void ThreadManager::collectAllChassisVpd()
{
    // Get the chassis to motherboard EEPROM path map from ConfigManager
    const auto& l_chassisToMotherboardEepromMap =
        m_configManager->getChassisToMotherboardEepromMap();

    // Get the chassisId to json map for chassis-specific configuration
    const auto& l_chassisIdToJsonMap = m_configManager->getChassisIdToJsonMap();

    if (l_chassisToMotherboardEepromMap.empty() || l_chassisIdToJsonMap.empty())
    {
        m_logger->logMessage(
            "Either chassisToEeprom map or chassisIdToJson map is empty. Nothing to collect.");
        return;
        // ToDo: colllection status needs to be updated at this point.
    }

    // Update the chassis count
    m_chassisCount = l_chassisToMotherboardEepromMap.size();

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

            std::thread{[l_eepromPath, l_chassisJson, l_chassisId, this]() {
                // Create a local Worker instance for this thread
                Worker l_threadWorker;

                uint16_t l_errCode = 0;
                auto [l_isPresent, l_collectionStatus] =
                    l_threadWorker.collectFruVpd(l_eepromPath, l_chassisJson,
                                                 l_errCode);

                updateSystemView(l_chassisId, l_eepromPath, l_chassisJson,
                                 l_isPresent);

                {
                    std::lock_guard<std::mutex> l_lock(m_mutex);
                    m_chassisTasks.push(std::make_tuple(
                        l_isPresent, l_eepromPath, l_chassisJson));
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

void ThreadManager::collectAllFruVpd()
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

    std::thread{[this]() {
        collectAllChassisVpd();

        processChassisResults();

        // ToDo: update collection status as completed
    }}.detach();

    m_logger->logMessage("All FRUs VPD collection initiated.",
                         PlaceHolder::COLLECTION);
}

void ThreadManager::updateSystemView(
    const std::string& i_chassisId, const std::string& i_eepromPath,
    const nlohmann::json& i_chassisJson, const bool i_isPresent) noexcept
{
    uint16_t l_errCode = 0;
    const std::string& l_invPath = jsonUtility::getInventoryObjPathFromJson(
        i_chassisJson, i_eepromPath, l_errCode);

    if (l_errCode || l_invPath.empty())
    {
        m_logger->logMessage(
            std::format("Failed to get inventory path for EEPROM {}, error: {}",
                        i_eepromPath, commonUtility::getErrCodeMsg(l_errCode)));
    }

    {
        std::lock_guard<std::mutex> l_lock(m_mutex);
        m_chassisStateMap.emplace(i_chassisId,
                                  std::make_pair(l_invPath, i_isPresent));
    }
}

void ThreadManager::processChassisResults() noexcept
{
    /**
     * @todo Complete asynchronous chassis result processing flow:
     * 1. Wait in a loop until all chassis motherboard VPD and its FRUs VPD
     * collections are completed.
     * 2. Launch chassis specific FRU VPD collection thread pool(launchFruCollectionPool),
     * to collect its FRUs VPD if that chassis is present.
     * 3. Update chassis counter and frus counter accordingly.
     */
}

void ThreadManager::launchFruCollectionPool(
    [[maybe_unused]] const std::string& i_chassisEeepromPath,
    [[maybe_unused]] const nlohmann::json& i_chassisJson) noexcept
{
    /**
     * @todo
     * 1. create iterator to chassis based json object for i_chassisJson
     * 2. Launch threads to collect chassis based FRUs VPD using created
     * iterator, and skip collecting VPD of i_chassisEeepromPath again.
     */
}
} // namespace vpd
