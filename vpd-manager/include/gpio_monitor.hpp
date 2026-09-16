#pragma once

#include "config_manager.hpp"
#include "utility/common_utility.hpp"
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
     * @param[in] fruPath - EEPROM path of the FRU.
     * @param[in] configManager - Pointer to Config manager object.
     * @param[in] ioContext - pointer to the io context object.
     *
     * @throw std::runtime_error
     */
    GpioEventHandler(
        const std::string fruPath,
        const std::shared_ptr<ConfigManager>& configManager,
        const std::shared_ptr<boost::asio::io_context>& ioContext) :
        fruPath(fruPath), configManager(configManager)
    {
        if (this->fruPath.empty())
        {
            throw std::invalid_argument(
                "FRU Path can't be empty for GpioEventHandler instantiation");
        }

        if (!this->configManager)
        {
            throw std::invalid_argument(
                "ConfigManager cannot be null. It is mandatory for GpioEventHandler instantiation");
        }

        if (!ioContext)
        {
            throw std::invalid_argument(
                "ASIO ioContext can not be null. It is mandatory for GpioEventHandle instantiation");
        }

        const auto configJsonResult =
            this->configManager->getJsonObj(this->fruPath);

        if (!configJsonResult.has_value())
        {
            throw std::runtime_error(std::format(
                "Path {} not found in JSON, can't instantiate GpioEventHandler. Error: {}",
                this->fruPath,
                commonUtility::getErrCodeMsg(configJsonResult.error())));
        }

        configJson = configJsonResult.value().get();
        setEventHandlerForGpioPresence(ioContext);
    }

  private:
    /**
     * @brief API to take action based on GPIO presence pin value.
     *
     * This API takes action based on the change in the presence pin value.
     * It performs deletion of FRU VPD if FRU is not present, otherwise performs
     * VPD collection if FRU gets added.
     *
     * @param[in] isFruPresent - Holds the present status of the FRU.
     */
    void handleChangeInGpioPin(const bool& isFruPresent);

    /**
     * @brief An API to set event handler for FRUs GPIO presence.
     *
     * An API to set timer to call event handler to detect GPIO presence
     * of the FRU.
     *
     * @param[in] ioContext - pointer to io context object
     */
    void setEventHandlerForGpioPresence(
        const std::shared_ptr<boost::asio::io_context>& ioContext);

    /**
     * @brief API to handle timer expiry.
     *
     * This API handles timer expiry and checks on the GPIO presence state,
     * takes action if there is any change in the GPIO presence value.
     *
     * @param[in] errorCode - Error Code
     * @param[in] timerObj - Pointer to timer Object.
     */
    void handleTimerExpiry(
        const boost::system::error_code& errorCode,
        const std::shared_ptr<boost::asio::steady_timer>& timerObj);

    const std::string fruPath;
    const std::shared_ptr<ConfigManager>& configManager;

    // Preserves the GPIO pin value to compare. Default value is false.
    bool prevPresencePinValue = false;

    // Chassis based JSON
    nlohmann::json configJson;
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
     * @param[in] configManager - pointer to Config manager Object.
     * @param[in] ioContext - pointer to IO context object.
     *
     */
    GpioMonitor(
        const std::shared_ptr<ConfigManager>& configManager,
        const std::shared_ptr<boost::asio::io_context>& ioContext) noexcept
    {
        try
        {
            if (!configManager)
            {
                throw std::invalid_argument(
                    "ConfigManager cannot be null. It is mandatory for GpioMonitor instantiation");
            }

            if (!ioContext)
            {
                throw std::invalid_argument(
                    "ASIO ioContext can not be null. It is mandatory for GpioMonitor instantiation");
            }

            initHandlerForGpio(ioContext, configManager);
        }
        catch (const std::exception& exception)
        {
            EventLogger::createSyncPel(
                types::ErrorType::InternalFailure, types::SeverityType::Warning,
                __FILE__, __FUNCTION__, 0,
                "Gpio Monitoring can't be instantiated. Error: " +
                    std::string(exception.what()),
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
     * @param[in] ioContext - Pointer to IO context object.
     * @param[in] configManager - Pointer to Config manager object.
     *
     * @throw std::runtime_error
     */
    void initHandlerForGpio(
        const std::shared_ptr<boost::asio::io_context>& ioContext,
        const std::shared_ptr<ConfigManager>& configManager);

    // Array of event handlers for all the attachable FRUs.
    std::vector<std::shared_ptr<GpioEventHandler>> gpioEventHandlerObjects;
};
} // namespace vpd
