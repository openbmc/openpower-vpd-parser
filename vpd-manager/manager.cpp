#include "config.h"

#include "manager.hpp"

#include "editor_impl.hpp"
#include "impl.hpp"
#include "ipz_parser.hpp"
#include "parser_factory.hpp"
#include "reader_impl.hpp"
#include "utils.hpp"

#include <fstream>

using namespace openpower::vpd::constants;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::manager::editor;
using namespace openpower::vpd::manager::reader;

namespace openpower
{
namespace vpd
{
namespace manager
{
Manager::Manager(sdbusplus::bus::bus&& bus, const char* busName,
                 const char* objPath, const char* /*iFace*/) :
    ServerObject<ManagerIface>(bus, objPath),
    _bus(std::move(bus)), _manager(_bus, objPath)
{
    _bus.request_name(busName);
}

void Manager::run()
{
    try
    {
        processJSON();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    while (true)
    {
        _bus.process_discard();

        // wait for event
        _bus.wait();
    }
}

void Manager::processJSON()
{
    getInvToEepromMap(frus);
    getLocationCodeToInvMap(fruLocationCode);
}

void Manager::writeKeyword(const sdbusplus::message::object_path path,
                           const std::string recordName,
                           const std::string keyword, const Binary value)
{
    try
    {
        if (frus.find(path) == frus.end())
        {
            throw std::runtime_error("Inventory path not found");
        }

        inventory::Path vpdFilePath = frus.find(path)->second.first;

        // instantiate editor class to update the data
        EditorImpl edit(vpdFilePath, jsonFile, recordName, keyword, path);
        edit.updateKeyword(value, true);

        // if it is a motehrboard FRU need to check for location expansion
        if (frus.find(path)->second.second)
        {
            if (recordName == "VCEN" && (keyword == "FC" || keyword == "SE"))
            {
                edit.expandLocationCode("fcs");
            }
            else if (recordName == "VSYS" &&
                     (keyword == "TM" || keyword == "SE"))
            {
                edit.expandLocationCode("mts");
            }
        }

        return;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

ListOfPaths
    Manager::getFRUsByUnexpandedLocationCode(const LocationCode locationCode,
                                             const NodeNumber nodeNumber)
{
    ReaderImpl read;
    return read.getFrusAtLocation(locationCode, nodeNumber, fruLocationCode);
}

ListOfPaths
    Manager::getFRUsByExpandedLocationCode(const LocationCode locationCode)
{
    ReaderImpl read;
    return read.getFRUsByExpandedLocationCode(locationCode, fruLocationCode);
}

LocationCode Manager::getExpandedLocationCode(const LocationCode locationCode,
                                              const NodeNumber nodeNumber)
{
    ReaderImpl read;
    return read.getExpandedLocationCode(locationCode, nodeNumber,
                                        fruLocationCode);
}
} // namespace manager
} // namespace vpd
} // namespace openpower
