#include "gpio_monitor.hpp"

#include "constants.hpp"
#include "error_codes.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/json_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <gpiod.hpp>

namespace vpd
{
void GpioEventHandler::handleChangeInGpioPin(const bool& isFruPresent)
{
    try
    {
        uint16_t errCode = 0;

        if (isFruPresent)
        {
            auto [isPresent, collectionStatus] =
                Worker{}.collectFruVpd(fruPath, configJson, errCode);

            if (errCode)
            {
                Logger::getLoggerInstance()->logMessage(std::format(
                    "Failed to collect FRU VPD for EEPROM [{}]. Present: {}, Status: {}, Error : {}",
                    fruPath, isPresent, collectionStatus,
                    commonUtility::getErrCodeMsg(errCode)));
            }
        }
        else
        {
            const std::string invPath =
                jsonUtility::getInventoryObjPathFromJson(fruPath, errCode);

            if (errCode)
            {
                throw std::runtime_error(
                    "Failed to get inventory path from JSON, error : " +
                    commonUtility::getErrCodeMsg(errCode));
            }

            Worker{}.deleteFruVpd(configJson, invPath);
        }
    }
    catch (std::exception& exception)
    {
        Logger::getLoggerInstance()->logMessage(std::string(exception.what()));
    }
}

void GpioEventHandler::handleTimerExpiry(
    const boost::system::error_code& errorCode,
    const std::shared_ptr<boost::asio::steady_timer>& timerObj)
{
    if (errorCode == boost::asio::error::operation_aborted)
    {
        Logger::getLoggerInstance()->logMessage("Timer aborted for GPIO pin");
        return;
    }

    if (errorCode)
    {
        Logger::getLoggerInstance()->logMessage(
            "Timer wait failed for gpio pin" + std::string(errorCode.message()));
        return;
    }

    uint16_t errCode = 0;
    bool currentPresencePinValue = jsonUtility::processGpioPresenceTag(
        fruPath, "pollingRequired", "hotPlugging", errCode);

    if (errCode && errCode != error_code::DEVICE_NOT_PRESENT)
    {
        Logger::getLoggerInstance()->logMessage(
            "processGpioPresenceTag returned false for FRU [" + fruPath +
            "] Due to error. Reason: " +
            commonUtility::getErrCodeMsg(errCode));
    }

    if (prevPresencePinValue != currentPresencePinValue)
    {
        prevPresencePinValue = currentPresencePinValue;
        handleChangeInGpioPin(currentPresencePinValue);
    }

    timerObj->expires_at(std::chrono::steady_clock::now() +
                         std::chrono::seconds(constants::VALUE_5));
    timerObj->async_wait(
        boost::bind(&GpioEventHandler::handleTimerExpiry, this,
                    boost::asio::placeholders::error, timerObj));
}

void GpioEventHandler::setEventHandlerForGpioPresence(
    const std::shared_ptr<boost::asio::io_context>& ioContext)
{
    uint16_t errCode = 0;
    prevPresencePinValue = jsonUtility::processGpioPresenceTag(
        fruPath, "pollingRequired", "hotPlugging", errCode);

    if (errCode && errCode != error_code::DEVICE_NOT_PRESENT)
    {
        Logger::getLoggerInstance()->logMessage(
            "processGpioPresenceTag returned false for FRU [" + fruPath +
            "] Due to error. Reason: " +
            commonUtility::getErrCodeMsg(errCode));
    }

    static std::vector<std::shared_ptr<boost::asio::steady_timer>> timers;

    auto timerObj = make_shared<boost::asio::steady_timer>(
        *ioContext, std::chrono::seconds(constants::VALUE_5));

    timerObj->async_wait(
        boost::bind(&GpioEventHandler::handleTimerExpiry, this,
                    boost::asio::placeholders::error, timerObj));

    timers.push_back(timerObj);
}

void GpioMonitor::initHandlerForGpio(
    const std::shared_ptr<boost::asio::io_context>& ioContext,
    const std::shared_ptr<ConfigManager>& configManager)
{
    if (!configManager)
    {
        throw std::invalid_argument(
            "Error: Config manager is null, can't process initHandlerForGpio.");
    }

    uint16_t errCode = 0;
    std::vector<std::string> gpioPollingRequiredFrusList =
        jsonUtility::getListOfGpioPollingFrus(errCode);

    if (errCode)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to get list of frus required for gpio polling. Error : " +
            commonUtility::getErrCodeMsg(errCode));
        return;
    }

    for (const auto& fruPath : gpioPollingRequiredFrusList)
    {
        std::shared_ptr<GpioEventHandler> gpioEventHandlerObj =
            std::make_shared<GpioEventHandler>(fruPath, configManager,
                                               ioContext);

        gpioEventHandlerObjects.push_back(gpioEventHandlerObj);
    }
}
} // namespace vpd
