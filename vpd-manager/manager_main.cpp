#include "config.h"

#include "manager.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

int main(int /*argc*/, char** /*argv*/)
{
    try
    {
        auto io_con = std::make_shared<boost::asio::io_context>();
        auto connection =
            std::make_shared<sdbusplus::asio::connection>(*io_con);
        connection->request_name(BUSNAME);

        auto server = sdbusplus::asio::object_server(connection);

        std::shared_ptr<sdbusplus::asio::dbus_interface> interface =
            server.add_interface(OBJPATH, IFACE);

        auto vpdManager = make_shared<openpower::vpd::manager::Manager>(
            io_con, interface, connection);
        interface->initialize();

        // Start event loop.
        io_con->run();

        exit(EXIT_SUCCESS);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    exit(EXIT_FAILURE);
}
