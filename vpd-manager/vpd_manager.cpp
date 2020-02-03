#include "config.h"

#include "vpd_manager.hpp"

#include "parser.hpp"

#include <exception>
#include <iostream>
#include <vector>

namespace openpower
{
namespace vpd
{
namespace manager
{
Manager::Manager(sdbusplus::bus::bus&& bus, const char* busName,
                 const char* objPath, const char* iFace) :
    ServerObject<ManagerIface>(bus, objPath),
    _bus(std::move(bus)), _manager(_bus, objPath)
{
    _bus.request_name(busName);
}

void Manager::run()
{
    while (true)
    {
        try
        {
            _bus.process_discard();
            // wait for event
            _bus.wait();
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << "\n";
        }
    }
}

void Manager::writeKeyword(const sdbusplus::message::object_path path,
                           const std::string recordName,
                           const std::string keyword, const Binary value)
{
    // implements the interface to write keyword VPD data
}

std::vector<sdbusplus::message::object_path>
    Manager::getFRUsByUnexpandedLocationCode(const std::string locationCode,
                                             const uint16_t nodeNumber)
{
    // impplements the interface
}

std::vector<sdbusplus::message::object_path>
    Manager::getFRUsByExpandedLocationCode(const std::string locationCode)
{
    // implement the interface
}

std::string Manager::getExpandedLocationCode(const std::string locationCode,
                                             const uint16_t nodeNumber)
{
    // implement the interface
}
} // namespace manager
} // namespace vpd
} // namespace openpower
