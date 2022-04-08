#include "config.h"

#include "manager.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <sdbusplus/bus.hpp>

int main(int /*argc*/, char** /*argv*/)
{
    try
    {
        openpower::vpd::manager::Manager vpdManager(
            sdbusplus::bus::new_system(), OBJPATH, IFACE);
        vpdManager.run(BUSNAME);
        exit(EXIT_SUCCESS);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
    exit(EXIT_FAILURE);
}