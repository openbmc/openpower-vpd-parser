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
    collectAllChassisVpd();
}

void ThreadManager::updateSystemView(
    const std::string& i_chassisId, const std::string& i_eepromPath,
    const nlohmann::json& i_chassisJson, const bool i_isPresent) noexcept
{
    uint16_t l_errCode = 0;
    std::string l_invPath = jsonUtility::getInventoryObjPathFromJson(
        i_chassisJson, i_eepromPath, l_errCode);

    if (l_errCode || l_invPath.empty())
    {
        m_logger->logMessage(std::format(
            "Failed to update system view for {}, reason: Failed to get inventory path, error: {}",
            i_chassisId, commonUtility::getErrCodeMsg(l_errCode)));
    }
    else
    {
        std::lock_guard<std::mutex> l_lock(m_mutex);
        m_chassisStateMap.emplace(i_chassisId,
                                  std::make_pair(l_invPath, i_isPresent));
    }
}
} // namespace vpd
