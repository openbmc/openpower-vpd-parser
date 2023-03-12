#include "config.h"

#include "bios_handler.hpp"
#include "event_logger.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "manager.hpp"
#include "types.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>

/**
 * @brief Main function for VPD parser application.
 */
int main(int, char**)
{
    try
    {
        auto io_con = std::make_shared<boost::asio::io_context>();
        auto connection =
            std::make_shared<sdbusplus::asio::connection>(*io_con);
        auto server = sdbusplus::asio::object_server(connection);

        std::shared_ptr<sdbusplus::asio::dbus_interface> interface =
            server.add_interface(OBJPATH, IFACE);

        auto vpdManager = std::make_shared<vpd::Manager>(io_con, interface,
                                                         connection);

        // TODO: Take this under conditional compilation for IBM
        auto biosHandler =
            std::make_shared<vpd::BiosHandler<vpd::IbmBiosHandler>>(connection,
                                                                    vpdManager);

        interface->initialize();

        vpd::logging::logMessage("Start VPD-Manager event loop");

        // Grab the bus name
        connection->request_name(BUSNAME);

        // Start event loop.
        io_con->run();

        exit(EXIT_SUCCESS);
    }
    catch (const std::exception& l_ex)
    {
        if (typeid(l_ex) == typeid(vpd::JsonException))
        {
            // ToDo: Severity needs to be revisited.
            vpd::EventLogger::createAsyncPel(
                vpd::types::ErrorType::JsonFailure,
                vpd::types::SeverityType::Informational, __FILE__, __FUNCTION__,
                0,
                std::string("VPD Manager service failed with : ") + l_ex.what(),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        }
        else if (typeid(l_ex) == typeid(vpd::GpioException))
        {
            // ToDo: Severity needs to be revisited.
            vpd::EventLogger::createAsyncPel(
                vpd::types::ErrorType::GpioError,
                vpd::types::SeverityType::Informational, __FILE__, __FUNCTION__,
                0,
                std::string("VPD Manager service failed with : ") + l_ex.what(),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        }
        else if (typeid(l_ex) == typeid(sdbusplus::exception::SdBusError))
        {
            // ToDo: Severity needs to be revisited.
            vpd::EventLogger::createAsyncPel(
                vpd::types::ErrorType::DbusFailure,
                vpd::types::SeverityType::Informational, __FILE__, __FUNCTION__,
                0,
                std::string("VPD Manager service failed with : ") + l_ex.what(),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
        }
        else
        {
            // ToDo: Severity needs to be revisited.
            vpd::EventLogger::createAsyncPel(
                vpd::types::ErrorType::InvalidVpdMessage,
                vpd::types::SeverityType::Informational, __FILE__, __FUNCTION__,
                0,
                std::string("VPD Manager service failed with : ") + l_ex.what(),
                "BMC0001", std::nullopt, std::nullopt, std::nullopt);
        }
    }
    exit(EXIT_FAILURE);
}
