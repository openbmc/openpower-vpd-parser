#include "thread_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "utility/event_logger_utility.hpp"
#include "utility/vpd_specific_utility.hpp"
#include "worker.hpp"

#include <utility/json_utility.hpp>

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

    if (l_chassisToMotherboardEepromMap.empty() || l_chassisIdToJsonMap.empty())
    {
        std::string l_errorMsg =
            "chassisToEeprom map or chassisIdToJson map is empty. "
            "VPD collection cannot proceed due to missing system configuration.";
        throw JsonException(l_errorMsg);
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
            --m_chassisCount;

            // Update per-chassis collection status to Failed
            uint16_t l_errCode = 0;
            vpdSpecificUtility::setCollectionStatusProperty(
                l_eepromPath, types::VpdCollectionStatus::Failed,
                m_configManager->getJsonObj(), l_errCode);

            std::string l_msg =
                std::format("{} not found in chassis ID to JSON map. "
                            "Skipping motherboard VPD collection.",
                            l_chassisId);

            if (l_errCode)
            {
                l_msg += std::format(
                    " Additionally, failed to update collection status for EEPROM [{}], error: {}.",
                    l_eepromPath, commonUtility::getErrCodeMsg(l_errCode));
            }

            m_logger->logMessage(std::move(l_msg));
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
                    m_chassisResultQueue.push(std::make_tuple(
                        l_isPresent, l_eepromPath, l_chassisJson));
                    m_completionCv.notify_one();
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
            --m_chassisCount;
            m_logger->logMessage(std::format(
                "Failed to spawn thread for chassis [{}], EEPROM [{}]. "
                "Error: {}, Type: {}",
                l_chassisId, l_eepromPath, l_ex.what(), typeid(l_ex).name()));
        }
    }
}

void ThreadManager::collectAllFruVpd()
{
    updateOverallCollectionStatus(types::VpdCollectionStatus::InProgress);

    try
    {
        std::thread{[this]() {
            try
            {
                collectAllChassisVpd();

                processChassisResults();

                updateOverallCollectionStatus(
                    types::VpdCollectionStatus::Completed);
            }
            catch (const std::exception& l_ex)
            {
                updateOverallCollectionStatus(
                    types::VpdCollectionStatus::Failed);
                m_logger->logMessage(std::format(
                    "Collect all FRU VPD failed, reason: ", l_ex.what()));
            }
        }}.detach();

        m_logger->logMessage("All FRUs VPD collection initiated.",
                             PlaceHolder::COLLECTION);
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
    if (m_configManager->getChassisToMotherboardEepromMap().empty())
    {
        m_logger->logMessage("Chassis to motherboard EEPROM map is empty.");
        return;
    }

    // @toto: l_maxThreadsPerChassis should be calculated based on the number of
    // chassis present in the system.
    const size_t l_maxThreadsPerChassis = std::max<size_t>(
        1, constants::MAX_THREADS /
               m_configManager->getChassisToMotherboardEepromMap().size());

    while (true)
    {
        bool l_decChassisCnt{false};
        try
        {
            types::ChassisCollectionResult l_chassisResult;

            {
                std::unique_lock<std::mutex> l_lock(m_mutex);

                // Continue until all pending tasks are over
                m_completionCv.wait(l_lock, [this]() {
                    return (!m_chassisResultQueue.empty() ||
                            (!m_chassisCount && !m_frusCount));
                });

                // Exit when all chassis and FRU VPD collection is complete
                if (!m_chassisCount && !m_frusCount)
                {
                    return;
                }

                l_chassisResult = std::move(m_chassisResultQueue.front());
                m_chassisResultQueue.pop();
                l_decChassisCnt = true;
            }

            const auto& l_chassisEepromPath = std::get<1>(l_chassisResult);

            // Collect FRUs for present chassis
            if (std::get<0>(l_chassisResult))
            {
                const auto& l_chassisJson = std::get<2>(l_chassisResult);
                // ToDo: Update m_frusCount based on the chassis Json.

                launchFruCollectionPool(l_chassisEepromPath, l_chassisJson,
                                        l_maxThreadsPerChassis);
            }
            else
            {
                m_logger->logMessage(
                    std::format(
                        "Chassis [{}] is not present; skipping FRUs collection.",
                        l_chassisEepromPath),
                    PlaceHolder::COLLECTION);
            }
        }
        catch (const std::exception& l_ex)
        {
            m_logger->logMessage(std::format(
                "Error occurred while processing chassis result: {}",
                l_ex.what()));
        }

        // Decrement chassis count after actions on the chassis result are
        // complete
        if (l_decChassisCnt)
        {
            --m_chassisCount;
        }
    }
}

void ThreadManager::launchFruCollectionPool(
    const std::string& i_chassisEeepromPath,
    const nlohmann::json& i_chassisJson,
    const size_t i_maxThreadsPerChassis) noexcept
{
    bool l_anyThreadLaunched{false};

    try
    {
        // Create shared context for FRU collection thread pool
        auto l_fruThreadContext = std::make_shared<FruThreadContext>(
            i_chassisEeepromPath, i_chassisJson);

        // Launch thread pool for parallel FRU VPD collection
        for (size_t l_index = 0; l_index < i_maxThreadsPerChassis; ++l_index)
        {
            try
            {
                std::thread([this, i_chassisEeepromPath, l_fruThreadContext]() {
                    try
                    {
                        processFruCollection(l_fruThreadContext);
                    }
                    catch (const std::exception& l_ex)
                    {
                        m_logger->logMessage(std::format(
                            "Exception in FRU collection thread for chassis "
                            "[{}], error: {}",
                            i_chassisEeepromPath, l_ex.what()));
                    }
                }).detach();

                l_anyThreadLaunched = true;
            }
            catch (const std::exception& l_ex)
            {
                m_logger->logMessage(std::format(
                    "Failed to launch FRU collection thread #{} for chassis "
                    "[{}], error: {}",
                    l_index + 1, i_chassisEeepromPath, l_ex.what()));
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(std::format(
            "Failed to create FRU collection thread pool for chassis "
            "[{}], error: {}",
            i_chassisEeepromPath, l_ex.what()));
    }

    // Decrement the FRU counter if no threads were launched
    if (!l_anyThreadLaunched)
    {
        m_frusCount -= i_chassisJson["frus"].size() - constants::VALUE_1;
    }
}

void ThreadManager::processFruCollection(
    [[maybe_unused]] const std::shared_ptr<FruThreadContext>&
        i_fruThreadContext) noexcept
{
    /**
     * @todo:
     * - Retrieve the next available FRU from the shared context.
     * - Collect VPD for the retrieved FRU.
     * - Update the pending FRU count and notify the waiting thread.
     * - Continue until no unprocessed FRUs remain.
     *
     * @note Multiple threads run this function sharing the same FRU list.
     * @note The chassis EEPROM is excluded from processing since its VPD has
     * already been collected during chassis VPD collection.
     */
}

} // namespace vpd
