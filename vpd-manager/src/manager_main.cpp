#include "config.h"

#include "bios_handler.hpp"
#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "manager.hpp"
#include "types.hpp"
#include "utility/event_logger_utility.hpp"

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

        std::shared_ptr<sdbusplus::asio::dbus_interface> progressInf =
            server.add_interface(OBJPATH,
                                 vpd::constants::vpdCollectionInterface);

        auto vpdManager = std::make_shared<vpd::Manager>(
            io_con, interface, progressInf, connection);

        // TODO: Take this under conditional compilation for IBM
        [[maybe_unused]] auto biosHandler =
            std::make_shared<vpd::BiosHandler<vpd::IbmBiosHandler>>(
                connection, vpdManager);

        interface->initialize();
        progressInf->initialize();

        vpd::logging::logMessage("Start VPD-Manager event loop");

        // Grab the bus name
        connection->request_name(BUSNAME);

        // Start event loop.
        io_con->run();

        exit(EXIT_SUCCESS);
    }
    catch (const std::exception& l_ex)
    {
        vpd::logging::logMessage("VPD-Manager service failed to start.");
        vpd::EventLogger::createSyncPel(
            vpd::EventLogger::getErrorType(l_ex),
            vpd::types::SeverityType::Critical, __FILE__, __FUNCTION__, 0,
            vpd::EventLogger::getErrorMsg(l_ex), std::nullopt, std::nullopt,
            std::nullopt, std::nullopt);
    }
    exit(EXIT_FAILURE);
}
