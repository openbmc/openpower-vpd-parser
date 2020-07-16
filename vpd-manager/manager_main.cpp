#include "config.h"

#include "manager.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <sdbusplus/bus.hpp>

int main(int argc __attribute__((unused)), char* argv[] __attribute__((unused)))
{
    try
    {
        openpower::vpd::manager::Manager vpdManager(
            sdbusplus::bus::new_system(), BUSNAME, OBJPATH, IFACE);
        vpdManager.run();
        exit(EXIT_SUCCESS);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
    exit(EXIT_FAILURE);
}
