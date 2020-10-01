#include "config.h"

#include "manager.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <sdbusplus/bus.hpp>

int main(int argc, char* argv[])
{
    try
    {
        std::cout<<"Get vpdManager\n";
        openpower::vpd::manager::Manager vpdManager(
            sdbusplus::bus::new_system(), BUSNAME, OBJPATH, IFACE);
        std::cout<<"Running vpdManager\n";
        vpdManager.run();
        exit(EXIT_SUCCESS);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
    exit(EXIT_FAILURE);
}
