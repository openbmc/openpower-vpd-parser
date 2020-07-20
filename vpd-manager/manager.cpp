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
using namespace std;
using namespace openpower::vpd::parser;
using namespace openpower::vpd::ipz::parser;
using namespace openpower::vpd::parser::factory;

namespace openpower
{
namespace vpd
{
namespace manager
{
Manager::Manager(sdbusplus::bus::bus&& bus, const char* busName,
                 const char* objPath,
                 const char* iFace __attribute__((unused))) :
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
    std::ifstream json(INVENTORY_JSON, std::ios::binary);

    if (!json)
    {
        throw std::runtime_error("json file not found");
    }

    jsonFile = nlohmann::json::parse(json);
    if (jsonFile.find("frus") == jsonFile.end())
    {
        throw std::runtime_error("frus group not found in json");
    }

    const nlohmann::json& groupFRUS =
        jsonFile["frus"].get_ref<const nlohmann::json::object_t&>();
    for (const auto& itemFRUS : groupFRUS.items())
    {
        const std::vector<nlohmann::json>& groupEEPROM =
            itemFRUS.value().get_ref<const nlohmann::json::array_t&>();
        for (const auto& itemEEPROM : groupEEPROM)
        {
            bool isMotherboard = false;
            if (itemEEPROM["extraInterfaces"].find(
                    "xyz.openbmc_project.Inventory.Item.Board.Motherboard") !=
                itemEEPROM["extraInterfaces"].end())
            {
                isMotherboard = true;
            }
            frus.emplace(itemEEPROM["inventoryPath"]
                             .get_ref<const nlohmann::json::string_t&>(),
                         std::make_pair(itemFRUS.key(), isMotherboard));

            if (itemEEPROM["extraInterfaces"].find(LOCATION_CODE_INF) !=
                itemEEPROM["extraInterfaces"].end())
            {
                fruLocationCode.emplace(
                    itemEEPROM["extraInterfaces"][LOCATION_CODE_INF]
                              ["LocationCode"]
                                  .get_ref<const nlohmann::json::string_t&>(),
                    itemEEPROM["inventoryPath"]
                        .get_ref<const nlohmann::json::string_t&>());
            }
        }
    }
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
        edit.updateKeyword(value);

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

void Manager::fixBrokenEcc(const sdbusplus::message::object_path path)
{
    try
    {
        if (frus.find(path) == frus.end())
        {
            throw std::runtime_error("Inventory path not found");
        }

        inventory::Path vpdFilePath = (frus.find(path)->second).first;
        std::fstream vpdFileStream;
        char buf[2048];
        vpdFileStream.rdbuf()->pubsetbuf(buf, sizeof(buf));
        vpdFileStream.open(vpdFilePath,
                           std::ios::in | std::ios::out | std::ios::binary);

        if (!vpdFileStream)
        {
            throw std::runtime_error("Unable to open vpd file");
        }

        Binary vpdVector((std::istreambuf_iterator<char>(vpdFileStream)),
                         std::istreambuf_iterator<char>());

        ParserInterface* Iparser =
            ParserFactory::getParser(std::move(vpdVector), vpdFilePath);
        IpzVpdParser* ipzParser = dynamic_cast<IpzVpdParser*>(Iparser);
        try
        {
            if (ipzParser == nullptr)
            {
                throw std::runtime_error("Invalid cast");
            }

            auto vpdPtr = ipzParser->fixEcc();

            // closing and reopening the vpd file stream in truncate mode.
            vpdFileStream.close();

            std::fstream vpdFileStream;
            char buf[2048];
            vpdFileStream.rdbuf()->pubsetbuf(buf, sizeof(buf));
            vpdFileStream.open(vpdFilePath, std::ios::in | std::ios::out |
                                                std::ios::binary |
                                                std::ios::trunc);

            std::ostream_iterator<uint8_t> outputIterator(vpdFileStream);
            auto start = vpdPtr.begin();
            auto end = vpdPtr.end();
            std::copy(start, end, outputIterator);

            vpdFileStream.close();
        }
        catch (const std::exception& e)
        {
            if (ipzParser != nullptr)
            {
                delete ipzParser;
            }
            throw std::runtime_error(e.what());
        }
        vpdFileStream.close();
        delete ipzParser;
        ipzParser = nullptr;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}
} // namespace manager
} // namespace vpd
} // namespace openpower
