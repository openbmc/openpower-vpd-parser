#pragma once

#include "utility/event_logger_utility.hpp"
#include "worker.hpp"

#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <vector>

namespace vpd
{
/**
 * @brief class for GPIO event handling.
 *
 * Responsible for detecting events and handling them. It continuously
 * monitors the presence of the FRU. If it detects any change, performs
 * deletion of FRU VPD if FRU is not present, otherwise performs VPD
 * collection if FRU gets added.
 */
class GpioEventHandler
{
  public:
    GpioEventHandler() = delete;
    ~GpioEventHandler() = default;
    GpioEventHandler(const GpioEventHandler&) = delete;
    GpioEventHandler& operator=(const GpioEventHandler&) = delete;
    GpioEventHandler(GpioEventHandler&&) = delete;
    GpioEventHandler& operator=(GpioEventHandler&&) = delete;

    /**
     * @brief Constructor
     *
     * @param[in] i_fruPath - EEPROM path of the FRU.
     * @param[in] i_worker - pointer to the worker object.
     * @param[in] i_ioContext - pointer to the io context object
     *
     * @throw std::runtime_error
     */
    GpioEventHandler(
        const std::string i_fruPath, const std::shared_ptr<Worker>& i_worker,
        const std::shared_ptr<boost::asio::io_context>& i_ioContext) :
        m_fruPath(i_fruPath), m_worker(i_worker)
    {
        if (m_worker == nullptr)
        {
            throw std::runtime_error(
                "Worker not initialized in GPIO Event Handler");
        }

        setEventHandlerForGpioPresence(i_ioContext);
    }

  private:
    /**
     * @brief API to take action based on GPIO presence pin value.
     *
     * This API takes action based on the change in the presence pin value.
     * It performs deletion of FRU VPD if FRU is not present, otherwise performs
     * VPD collection if FRU gets added.
     *
     * @param[in] i_isFruPresent - Holds the present status of the FRU.
     */
    void handleChangeInGpioPin(const bool& i_isFruPresent);

    /**
     * @brief An API to set event handler for FRUs GPIO presence.
     *
     * An API to set timer to call event handler to detect GPIO presence
     * of the FRU.
     *
     * @param[in] i_ioContext - pointer to io context object
     */
    void setEventHandlerForGpioPresence(
        const std::shared_ptr<boost::asio::io_context>& i_ioContext);

    /**
     * @brief API to handle timer expiry.
     *
     * This API handles timer expiry and checks on the GPIO presence state,
     * takes action if there is any change in the GPIO presence value.
     *
     * @param[in] i_errorCode - Error Code
     * @param[in] i_timerObj - Pointer to timer Object.
     */
    void handleTimerExpiry(
        const boost::system::error_code& i_errorCode,
        const std::shared_ptr<boost::asio::steady_timer>& i_timerObj);

    const std::string m_fruPath;

    const std::shared_ptr<Worker>& m_worker;

    // Preserves the GPIO pin value to compare. Default value is false.
    bool m_prevPresencePinValue = false;
};

class GpioMonitor
{
  public:
    GpioMonitor() = delete;
    ~GpioMonitor() = default;
    GpioMonitor(const GpioMonitor&) = delete;
    GpioMonitor& operator=(const GpioMonitor&) = delete;
    GpioMonitor(GpioMonitor&&) = delete;
    GpioMonitor& operator=(GpioMonitor&&) = delete;

    /**
     * @brief constructor
     *
     * @param[in] i_sysCfgJsonObj - System config JSON Object.
     * @param[in] i_worker - pointer to the worker object.
     * @param[in] i_ioContext - pointer to IO context object.
     *
     */
    GpioMonitor(
        const nlohmann::json i_sysCfgJsonObj,
        const std::shared_ptr<Worker>& i_worker,
        const std::shared_ptr<boost::asio::io_context>& i_ioContext) noexcept :
        m_sysCfgJsonObj(i_sysCfgJsonObj)
    {
        try
        {
            if (!m_sysCfgJsonObj.empty())
            {
                initHandlerForGpio(i_ioContext, i_worker);
            }
            else
            {
                throw std::runtime_error(
                    "Gpio Monitoring can't be instantiated with empty config JSON");
            }
        }
        catch (const std::exception& l_ex)
        {
            EventLogger::createSyncPel(
                types::ErrorType::InternalFailure, types::SeverityType::Warning,
                __FILE__, __FUNCTION__, 0,
                "Gpio Monitoring can't be instantiated. Error: " +
                    std::string(l_ex.what()),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        }
    }

  private:
    /**
     * @brief API to instantiate GpioEventHandler for GPIO pins.
     *
     * This API will extract the GPIO information from system config JSON
     * and instantiate event handler for GPIO pins.
     *
     * @param[in] i_ioContext - Pointer to IO context object.
     * @param[in] i_worker - Pointer to worker class.
     *
     * @throw std::runtime_error
     */
    void initHandlerForGpio(
        const std::shared_ptr<boost::asio::io_context>& i_ioContext,
        const std::shared_ptr<Worker>& i_worker);

    // Array of event handlers for all the attachable FRUs.
    std::vector<std::shared_ptr<GpioEventHandler>> m_gpioEventHandlerObjects;

    const nlohmann::json& m_sysCfgJsonObj;
};
} // namespace vpd
