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
void GpioEventHandler::handleChangeInGpioPin(const bool& i_isFruPresent)
{
    try
    {
        nlohmann::json l_chassisBasedJsonObj{};
        if (m_configManager)
        {
            l_chassisBasedJsonObj = m_configManager->getJsonObj(m_fruPath);
        }

        Worker l_worker;

        if (i_isFruPresent)
        {
            uint16_t l_errCode = 0;
            auto [l_isPresent, l_collectionStatus] = l_worker.collectFruVpd(
                m_fruPath, l_chassisBasedJsonObj, l_errCode);

            if (l_errCode)
            {
                logging::logMessage(std::format(
                    "Failed to collect FRU VPD for EEPROM [{}]. Present: {}, Status: {}, Error : {}",
                    m_fruPath, l_isPresent, l_collectionStatus,
                    commonUtility::getErrCodeMsg(l_errCode)));
            }
        }
        else
        {
            uint16_t l_errCode = 0;
            std::string l_invPath = jsonUtility::getInventoryObjPathFromJson(
                l_chassisBasedJsonObj, m_fruPath, l_errCode);

            if (l_errCode)
            {
                throw std::runtime_error(
                    "Failed to get inventory path from JSON, error : " +
                    commonUtility::getErrCodeMsg(l_errCode));
            }

            l_worker.deleteFruVpd(l_invPath);
        }
    }
    catch (std::exception& l_ex)
    {
        logging::logMessage(std::string(l_ex.what()));
    }
}

void GpioEventHandler::handleTimerExpiry(
    const boost::system::error_code& i_errorCode,
    const std::shared_ptr<boost::asio::steady_timer>& i_timerObj)
{
    if (i_errorCode == boost::asio::error::operation_aborted)
    {
        logging::logMessage("Timer aborted for GPIO pin");
        return;
    }

    if (i_errorCode)
    {
        logging::logMessage("Timer wait failed for gpio pin" +
                            std::string(i_errorCode.message()));
        return;
    }

    uint16_t l_errCode = 0;

    bool l_currentPresencePinValue = jsonUtility::processGpioPresenceTag(
        m_sysConfigJsonObj, m_fruPath, "pollingRequired", "hotPlugging",
        l_errCode);

    if (l_errCode && l_errCode != error_code::DEVICE_NOT_PRESENT)
    {
        logging::logMessage("processGpioPresenceTag returned false for FRU [" +
                            m_fruPath + "] Due to error. Reason: " +
                            commonUtility::getErrCodeMsg(l_errCode));
    }

    if (m_prevPresencePinValue != l_currentPresencePinValue)
    {
        m_prevPresencePinValue = l_currentPresencePinValue;
        handleChangeInGpioPin(l_currentPresencePinValue);
    }

    i_timerObj->expires_at(std::chrono::steady_clock::now() +
                           std::chrono::seconds(constants::VALUE_5));
    i_timerObj->async_wait(
        boost::bind(&GpioEventHandler::handleTimerExpiry, this,
                    boost::asio::placeholders::error, i_timerObj));
}

void GpioEventHandler::setEventHandlerForGpioPresence(
    const std::shared_ptr<boost::asio::io_context>& i_ioContext)
{
    uint16_t l_errCode = 0;

    if (m_configManager)
    {
        m_sysConfigJsonObj = m_configManager->getJsonObj();
    }

    m_prevPresencePinValue = jsonUtility::processGpioPresenceTag(
        m_sysConfigJsonObj, m_fruPath, "pollingRequired", "hotPlugging",
        l_errCode);

    if (l_errCode && l_errCode != error_code::DEVICE_NOT_PRESENT)
    {
        logging::logMessage("processGpioPresenceTag returned false for FRU [" +
                            m_fruPath + "] Due to error. Reason: " +
                            commonUtility::getErrCodeMsg(l_errCode));
    }

    static std::vector<std::shared_ptr<boost::asio::steady_timer>> l_timers;

    auto l_timerObj = make_shared<boost::asio::steady_timer>(
        *i_ioContext, std::chrono::seconds(constants::VALUE_5));

    l_timerObj->async_wait(
        boost::bind(&GpioEventHandler::handleTimerExpiry, this,
                    boost::asio::placeholders::error, l_timerObj));

    l_timers.push_back(l_timerObj);
}

void GpioMonitor::initHandlerForGpio(
    const std::shared_ptr<boost::asio::io_context>& i_ioContext,
    const std::shared_ptr<ConfigManager>& i_configManager)
{
    uint16_t l_errCode = 0;
    nlohmann::json l_configJsonObj{};
    if (i_configManager)
    {
        l_configJsonObj = i_configManager->getJsonObj();
    }

    std::vector<std::string> l_gpioPollingRequiredFrusList =
        jsonUtility::getListOfGpioPollingFrus(l_configJsonObj, l_errCode);

    if (l_errCode)
    {
        logging::logMessage(
            "Failed to get list of frus required for gpio polling. Error : " +
            commonUtility::getErrCodeMsg(l_errCode));
        return;
    }

    for (const auto& l_fruPath : l_gpioPollingRequiredFrusList)
    {
        std::shared_ptr<GpioEventHandler> l_gpioEventHandlerObj =
            std::make_shared<GpioEventHandler>(l_fruPath, i_configManager,
                                               i_ioContext);

        m_gpioEventHandlerObjects.push_back(l_gpioEventHandlerObj);
    }
}
} // namespace vpd
