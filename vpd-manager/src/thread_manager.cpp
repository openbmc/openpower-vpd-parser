#include "thread_manager.hpp"

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "utility/event_logger_utility.hpp"
#include "utility/vpd_specific_utility.hpp"
#include "worker.hpp"

#include <utility/json_utility.hpp>

#include <chrono>
#include <format>
#include <thread>

namespace vpd
{

ThreadManager::ThreadManager(
    const std::shared_ptr<ConfigManager>& configManager,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& progressInterface) :
    configManager(configManager), progressInterface(progressInterface),
    logger(Logger::getLoggerInstance())
{
    if (!configManager)
    {
        throw std::invalid_argument(
            "ConfigManager cannot be null - it is mandatory for ThreadManager instantiation");
    }

    if (!progressInterface)
    {
        throw std::invalid_argument(
            "Progress interface can not be null, it is mandatory for ThreadManager instantiation");
    }
}

void ThreadManager::updateOverallCollectionStatus(
    const types::VpdCollectionStatus status) const noexcept
{
    progressInterface->set_property(
        "Status",
        types::CommonProgress::convertOperationStatusToString(status));
    progressInterface->signal_property("Status");
}

void ThreadManager::collectAllChassisVpd()
{
    // Get the chassis to motherboard EEPROM path map from ConfigManager
    const auto& chassisToMotherboardEepromMap =
        configManager->getChassisToMotherboardEepromMap();

    // Get the chassisId to json map for chassis-specific configuration
    const auto& chassisIdToJsonMap = configManager->getChassisIdToJsonMap();

    if (chassisToMotherboardEepromMap.empty() || chassisIdToJsonMap.empty())
    {
        std::string errorMsg =
            "chassisToEeprom map or chassisIdToJson map is empty. "
            "VPD collection cannot proceed due to missing system configuration.";
        throw JsonException(errorMsg);
    }

    // Update the chassis count
    chassisCount = chassisToMotherboardEepromMap.size();

    logger->logMessage(
        std::format(
            "Starting multi-threaded motherboard VPD collection for {} chassis",
            chassisToMotherboardEepromMap.size()),
        PlaceHolder::COLLECTION);

    for (const auto& [chassisId, eepromPath] : chassisToMotherboardEepromMap)
    {
        auto chassisToJsonItr = chassisIdToJsonMap.find(chassisId);
        if (chassisToJsonItr == chassisIdToJsonMap.end())
        {
            --chassisCount;

            // Update per-chassis collection status to Failed
            uint16_t errCode = 0;
            vpdSpecificUtility::setCollectionStatusProperty(
                eepromPath, types::VpdCollectionStatus::Failed,
                configManager->getJsonObj().value().get(), errCode);

            std::string msg =
                std::format("{} not found in chassis ID to JSON map. "
                            "Skipping motherboard VPD collection.",
                            chassisId);

            if (errCode)
            {
                msg += std::format(
                    " Additionally, failed to update collection status for EEPROM [{}], error: {}.",
                    eepromPath, commonUtility::getErrCodeMsg(errCode));
            }

            logger->logMessage(std::move(msg));
            continue;
        }

#ifdef IBM_SYSTEM
        // Skip collecting system VPD path again
        if (eepromPath == SYSTEM_VPD_FILE_PATH)
        {
            handleChassisHavingSystemVpd(chassisToJsonItr->second, chassisId,
                                         eepromPath);
            continue;
        }
#endif

        logger->logMessage(
            std::format(
                "Spawning thread for chassis [{}] with EEPROM path [{}]",
                chassisId, eepromPath),
            PlaceHolder::COLLECTION);

        try
        {
            const nlohmann::json& chassisJson = chassisToJsonItr->second;

            std::thread{[eepromPath, chassisJson, chassisId, this]() {
                // Create a local Worker instance for this thread
                Worker threadWorker;

                uint16_t errCode = 0;
                auto [isPresent, collectionStatus] = threadWorker.collectFruVpd(
                    eepromPath, chassisJson, errCode);

                updateSystemView(chassisId, eepromPath, isPresent);

                {
                    std::lock_guard<std::mutex> lock(mutex);
                    chassisResultQueue.push(
                        std::make_tuple(isPresent, eepromPath, chassisJson));
                    completionCv.notify_one();
                }

                logger->logMessage(
                    std::format("Completed VPD collection for EEPROM [{}]. "
                                "Present: {}, Status: {}, ErrorCode: {}",
                                eepromPath, isPresent, collectionStatus,
                                errCode),
                    PlaceHolder::COLLECTION);
            }}.detach();
            // ToDo:- this detach mode is for time being, we need to update the
            // system view post collection.
        }
        catch (const std::exception& ex)
        {
            --chassisCount;
            logger->logMessage(std::format(
                "Failed to spawn thread for chassis [{}], EEPROM [{}]. "
                "Error: {}, Type: {}",
                chassisId, eepromPath, ex.what(), typeid(ex).name()));
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
                auto start = std::chrono::steady_clock::now();
                collectAllChassisVpd();

                bool result = processChassisResults();

                const auto completionStatus =
                    (result ? types::VpdCollectionStatus::Completed
                            : types::VpdCollectionStatus::Failed);
                updateOverallCollectionStatus(completionStatus);

                const auto elapsedSeconds =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - start)
                        .count();
                logger->logMessage(std::format(
                    "Total time taken for all FRU VPD collection = {} seconds",
                    elapsedSeconds));
            }
            catch (const std::exception& ex)
            {
                updateOverallCollectionStatus(
                    types::VpdCollectionStatus::Failed);
                logger->logMessage(std::format(
                    "Collect all FRU VPD failed, reason: {}", ex.what()));
            }
        }}.detach();

        logger->logMessage("All FRUs VPD collection initiated.",
                           PlaceHolder::COLLECTION);
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(
            std::format("VPD collection failed with exception: {}, Type: {}",
                        ex.what(), typeid(ex).name()),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::InternalFailure,
                                types::SeverityType::Critical, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});

        updateOverallCollectionStatus(types::VpdCollectionStatus::Failed);
    }
}

void ThreadManager::updateSystemView(const std::string& chassisId,
                                     const std::string& eepromPath,
                                     const bool isPresent) noexcept
{
    uint16_t errCode = 0;
    const std::string& invPath =
        jsonUtility::getInventoryObjPathFromJson(eepromPath, errCode);

    if (errCode || invPath.empty())
    {
        logger->logMessage(
            std::format("Failed to get inventory path for EEPROM {}, error: {}",
                        eepromPath, commonUtility::getErrCodeMsg(errCode)));
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        chassisStateMap.emplace(chassisId, std::make_pair(invPath, isPresent));
    }
}

bool ThreadManager::processChassisResults() noexcept
{
    if (configManager->getChassisToMotherboardEepromMap().empty())
    {
        logger->logMessage("Chassis to motherboard EEPROM map is empty.");
        return false;
    }

    // @todo: maxThreadsPerChassis should be calculated based on the number of
    // chassis present in the system.
    const size_t maxThreadsPerChassis = std::max<size_t>(
        1, constants::MAX_THREADS /
               configManager->getChassisToMotherboardEepromMap().size());

    while (true)
    {
        bool decChassisCnt{false};
        try
        {
            types::ChassisCollectionResult chassisResult;

            {
                std::unique_lock<std::mutex> lock(mutex);

                // Continue until all pending tasks are over, or timeout expires
                const bool timedOut = !completionCv.wait_for(
                    lock,
                    std::chrono::seconds(constants::VPD_COLLECTION_TIMEOUT_SEC),
                    [this]() {
                        return (!chassisResultQueue.empty() ||
                                (!chassisCount && !frusCount));
                    });

                if (timedOut)
                {
                    logger->logMessage(
                        std::format(
                            "VPD collection timed out after {} seconds. "
                            "Pending chassis: {}, pending FRUs: {}. Exiting.",
                            constants::VPD_COLLECTION_TIMEOUT_SEC,
                            chassisCount.load(), frusCount.load()),
                        PlaceHolder::ASYNC_PEL,
                        types::PelInfoTuple{types::ErrorType::FirmwareError,
                                            types::SeverityType::Warning, 0,
                                            std::nullopt, std::nullopt,
                                            std::nullopt, std::nullopt,
                                            std::nullopt});
                    return false;
                }

                // Exit when all chassis and FRU VPD collection is complete
                if (!chassisCount && !frusCount)
                {
                    return true;
                }

                chassisResult = std::move(chassisResultQueue.front());
                chassisResultQueue.pop();
                decChassisCnt = true;
            }

            const auto& chassisEepromPath = std::get<1>(chassisResult);
            const auto& chassisJson = std::get<2>(chassisResult);

            if (chassisJson["frus"].size() <= constants::VALUE_1)
            {
                logger->logMessage(std::format(
                    "There are no FRUs to collect VPD, for the chassis [{}].",
                    chassisEepromPath));
            }

            // Collect FRUs for present chassis
            else if (std::get<0>(chassisResult))
            {
                // Increment the FRU counter before staring FRUs VPD collection.
                // Exclude chassis/motherboard VPD, which was already collected
                frusCount += chassisJson["frus"].size() - constants::VALUE_1;

                launchFruCollectionPool(chassisEepromPath, chassisJson,
                                        maxThreadsPerChassis);
            }
            else
            {
                logger->logMessage(
                    std::format(
                        "Chassis [{}] is not present; skipping FRUs collection.",
                        chassisEepromPath),
                    PlaceHolder::COLLECTION);
            }
        }
        catch (const std::exception& ex)
        {
            logger->logMessage(std::format(
                "Error occurred while processing chassis result: {}",
                ex.what()));
        }

        // Decrement chassis count after actions on the chassis result are
        // complete
        if (decChassisCnt)
        {
            --chassisCount;
        }
    }
}

void ThreadManager::launchFruCollectionPool(
    const std::string& chassisEeepromPath, const nlohmann::json& chassisJson,
    const size_t maxThreadsPerChassis) noexcept
{
    bool anyThreadLaunched{false};

    try
    {
        // Create shared context for FRU collection thread pool
        auto fruThreadContext =
            std::make_shared<FruThreadContext>(chassisEeepromPath, chassisJson);

        // Launch thread pool for parallel FRU VPD collection
        for (size_t index = 0; index < maxThreadsPerChassis; ++index)
        {
            try
            {
                std::thread([this, chassisEeepromPath, fruThreadContext]() {
                    processFruCollection(fruThreadContext);
                }).detach();

                anyThreadLaunched = true;
            }
            catch (const std::exception& ex)
            {
                logger->logMessage(std::format(
                    "Failed to launch FRU collection thread #{} for chassis "
                    "[{}], error: {}",
                    index + 1, chassisEeepromPath, ex.what()));
            }
        }
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(std::format(
            "Failed to create FRU collection thread pool for chassis "
            "[{}], error: {}",
            chassisEeepromPath, ex.what()));
    }

    // Decrement the FRU counter if no threads were launched
    if (!anyThreadLaunched && chassisJson["frus"].size() > constants::VALUE_1)
    {
        frusCount -= chassisJson["frus"].size() - constants::VALUE_1;
        completionCv.notify_one();
    }
}

void ThreadManager::processFruCollection(
    const std::shared_ptr<FruThreadContext>& fruThreadContext) noexcept
{
    if (!fruThreadContext)
    {
        logger->logMessage(
            "Received null FRU thread context. Skipping FRU VPD collection.");
        return;
    }

    try
    {
        while (true)
        {
            // Get next FRU to process
            std::string fruPath = getNextFruPath(fruThreadContext);

            if (fruPath.empty())
            {
                break;
            }

            uint16_t errCode = 0;
            auto [isPresent, collectionStatus] = Worker{}.collectFruVpd(
                fruPath, fruThreadContext->chassisJson, errCode);

            // Update FRU count and notify waiting thread
            if (frusCount > 0)
            {
                --frusCount;
            }
            completionCv.notify_one();
        }
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(
            std::format("Exception in FRU collection thread for chassis "
                        "[{}], error: {}",
                        fruThreadContext->chassisEeepromPath, ex.what()));
    }
}

std::string ThreadManager::getNextFruPath(
    const std::shared_ptr<FruThreadContext>& fruThreadContext) const noexcept
{
    try
    {
        if (!fruThreadContext)
        {
            logger->logMessage("Invalid input is given");
            return std::string{};
        }

        std::string fruPath;

        std::lock_guard<std::mutex> lock(fruThreadContext->fruItrMutex);

        while (fruThreadContext->fruItr != fruThreadContext->frus.end())
        {
            fruPath = fruThreadContext->fruItr->first;
            ++fruThreadContext->fruItr;

            // Skip chassis EEPROM as it was already collected
            if (fruPath == fruThreadContext->chassisEeepromPath)
            {
                fruPath.clear();
                continue;
            }

            break;
        }

        return fruPath;
    }
    catch (const std::exception& ex)
    {
        logger->logMessage(std::format(
            "Error while getting next FRU path, reason: {}", ex.what()));
        return std::string{};
    }
}

#ifdef IBM_SYSTEM
void ThreadManager::handleChassisHavingSystemVpd(
    const nlohmann::json& chassisJson, const std::string& chassisId,
    const std::string& eepromPath) noexcept
{
    try
    {
        // Read Present property value from Dbus.
        auto kwdValueVariant = dbusUtility::readDbusProperty(
            chassisJson["frus"][eepromPath][0]["serviceName"],
            chassisJson["frus"][eepromPath][0]["inventoryPath"],
            constants::inventoryItemInf, "Present");

        bool isFruPresent = false;
        if (const auto value = std::get_if<bool>(&kwdValueVariant))
        {
            isFruPresent = *value;
        }
        else
        {
            logger->logMessage(std::format(
                "Invalid type received for Present property from D-Bus for inventory path: [{}], proceeding further by assuming chassis is absent.",
                std::string(
                    chassisJson["frus"][eepromPath][0]["inventoryPath"])));
        }

        updateSystemView(chassisId, eepromPath, isFruPresent);

        {
            std::lock_guard<std::mutex> lock(mutex);
            chassisResultQueue.push(
                std::make_tuple(isFruPresent, eepromPath, chassisJson));
            completionCv.notify_one();
        }
    }
    catch (const std::exception& ex)
    {
        --chassisCount;
        completionCv.notify_one();

        logger->logMessage(std::format(
            "Error while handling chassis with system VPD path, reason: {}",
            ex.what()));
    }
}
#endif

} // namespace vpd
