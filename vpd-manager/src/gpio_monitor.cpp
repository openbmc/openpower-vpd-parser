#include "gpio_monitor.hpp"

#include "constants.hpp"
#include "logger.hpp"
#include "types.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/json_utility.hpp"

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <gpiod.hpp>

namespace vpd
{
void GpioEventHandler::handleChangeInGpioPin(const bool& i_isFruPresent)
{
    try
    {
        if (i_isFruPresent)
        {
            types::VPDMapVariant l_parsedVpd =
                m_worker->parseVpdFile(m_fruPath);

            if (std::holds_alternative<std::monostate>(l_parsedVpd))
            {
                throw std::runtime_error("VPD parsing failed for " +
                                         std::string(m_fruPath));
            }

            types::ObjectMap l_dbusObjectMap;
            m_worker->populateDbus(l_parsedVpd, l_dbusObjectMap, m_fruPath);

            if (l_dbusObjectMap.empty())
            {
                throw std::runtime_error("Failed to create D-bus object map.");
            }

            if (!dbusUtility::callPIM(move(l_dbusObjectMap)))
            {
                throw std::runtime_error("call PIM failed");
            }
        }
        else
        {
            // TODO -- Add implementation to Delete FRU if FRU is not present.
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

    bool l_currentPresencePinValue = jsonUtility::processGpioPresenceTag(
        m_worker->getSysCfgJsonObj(), m_fruPath, "pollingRequired",
        "hotPlugging");

    if (m_prevPresencePinValue != l_currentPresencePinValue)
    {
        m_prevPresencePinValue = l_currentPresencePinValue;
        handleChangeInGpioPin(l_currentPresencePinValue);
    }

    i_timerObj->expires_at(std::chrono::steady_clock::now() +
                           std::chrono::seconds(constants::VALUE_5));
    i_timerObj->async_wait(boost::bind(&GpioEventHandler::handleTimerExpiry,
                                       this, boost::asio::placeholders::error,
                                       i_timerObj));
}

void GpioEventHandler::setEventHandlerForGpioPresence(
    const std::shared_ptr<boost::asio::io_context>& i_ioContext)
{
    m_prevPresencePinValue = jsonUtility::processGpioPresenceTag(
        m_worker->getSysCfgJsonObj(), m_fruPath, "pollingRequired",
        "hotPlugging");

    static std::vector<std::shared_ptr<boost::asio::steady_timer>> l_timers;

    auto l_timerObj = make_shared<boost::asio::steady_timer>(
        *i_ioContext, std::chrono::seconds(constants::VALUE_5));

    l_timerObj->async_wait(boost::bind(&GpioEventHandler::handleTimerExpiry,
                                       this, boost::asio::placeholders::error,
                                       l_timerObj));

    l_timers.push_back(l_timerObj);
}

void GpioMonitor::initHandlerForGpio(
    const std::shared_ptr<boost::asio::io_context>& i_ioContext,
    const std::shared_ptr<Worker>& i_worker)
{
    try
    {
        std::vector<std::string> l_gpioPollingRequiredFrusList =
            jsonUtility::getListOfGpioPollingFrus(m_sysCfgJsonObj);

        for (const auto& l_fruPath : l_gpioPollingRequiredFrusList)
        {
            std::shared_ptr<GpioEventHandler> l_gpioEventHandlerObj =
                std::make_shared<GpioEventHandler>(l_fruPath, i_worker,
                                                   i_ioContext);

            m_gpioEventHandlerObjects.push_back(l_gpioEventHandlerObj);
        }
    }
    catch (std::exception& l_ex)
    {
        // TODO log PEL for exception.
        logging::logMessage(l_ex.what());
    }
}
} // namespace vpd
