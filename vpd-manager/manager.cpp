#include "config.h"

#include "manager.hpp"

#include "editor_impl.hpp"
#include "parser.hpp"
#include "reader_impl.hpp"
#include "utils.hpp"

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
                 const char* objPath, const char* iFace) :
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
            bool isMotherBoard = false;
            if (itemEEPROM["extraInterfaces"].find(
                    "xyz.openbmc_project.Inventory.Item.Board.Motherboard") !=
                itemEEPROM["extraInterfaces"].end())
            {
                isMotherBoard = true;
            }
            frus.emplace(itemEEPROM["inventoryPath"]
                             .get_ref<const nlohmann::json::string_t&>(),
                         std::make_pair(itemFRUS.key(), isMotherBoard));

            fruLocationCode.emplace(
                itemEEPROM["extraInterfaces"][LOCATION_CODE_INF]["LocationCode"]
                    .get_ref<const nlohmann::json::string_t&>(),
                itemEEPROM["inventoryPath"]
                    .get_ref<const nlohmann::json::string_t&>());
        }
    }
}

string getSysPathForThisFruType(const string& moduleObjPath, const string& fruType)
{
    string fruVpdPath;

    //get all FRUs list
    for (const auto& eachFru : jsonFile["frus"].items())
    {
        bool moduleObjPathMatched = false;
        bool expectedFruFound = false;
        cout<<"checking "<<eachFru.key()<<"\n";

        for (const auto& eachInventory : eachFru.value())
        {
            const auto& thisObjectPath = eachInventory["inventoryPath"];
            cout<<"Processing for "<<thisObjectPath <<"\n";

            // "type" exists only in CPU module and FRU
            if (eachInventory.find("type") != eachInventory.end())
            {
                //If inventory type is fruAndModule then set flag
                if( eachInventory["type"] == fruType)
                {
                    expectedFruFound = true;
                }
            }

            if(thisObjectPath == moduleObjPath)
            {
                moduleObjPathMatched = true;
            }
        }

        //If condition satisfies then collect this sys path and exit
        if( expectedFruFound && moduleObjPathMatched)
        {
            fruVpdPath = eachFru.key();
            break;
        }
    }

    return fruVpdPath;
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

        // check If it is CpuModule, update vpdFilePath accordingly
        inventory::Path vpdFilePath = frus.find(path)->second;

        //Temp hardcoded list
        vector<string> commonIntRecordsList = {"VINI", "VR10"};

        if( jsonFile["frus"].find(vpdFilePath) == jsonFile["frus"].end())
        for (const auto& item : jsonFile["frus"][vpdFilePath])
        {
            if (item.find("type") != item.end())
            {
                if( item["type"] == "moduleOnly")
                {
                    //If requested Record is one among CI, then update FRU sys path, SPI2
                    if( find( commonIntRecordsList.begin(),commonIntRecordsList.end(), recordName) !=
                        commonIntRecordsList.end() )
                    {
                        vpdFilePath = getSysPathForThisFruType(path, "fruAndModule");
                    }
                    else    //update Module_only sys path, SPI6
                    {
                        vpdFilePath = getSysPathForThisFruType(path, "moduleOnly");
                    }
                }
            }
            //else go ahead with default vpdFilePath from fruMap
        }


        std::ifstream vpdStream(vpdFilePath, std::ios::binary);
        if (!vpdStream)
        {
            throw std::runtime_error("file not found");
        }

        // instantiate editor class to update the data
        EditorImpl edit(vpdFilePath, jsonFile, recordName, keyword);
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
                                             const Node nodeNumber)
{
    ReaderImpl read;
    return read.getFrusAtLocation(locationCode, nodeNumber, fruLocationCode);
}

ListOfPaths
    Manager::getFRUsByExpandedLocationCode(const LocationCode locationCode)
{
    ReaderImpl read;
    std::tuple<LocationCode, Node> locationAndNodePair =
        read.getCollapsedLocationCode(locationCode);

    return read.getFrusAtLocation(std::get<0>(locationAndNodePair),
                                  std::get<1>(locationAndNodePair),
                                  fruLocationCode);
}

LocationCode Manager::getExpandedLocationCode(const LocationCode locationCode,
                                              const Node nodeNumber)
{
    ReaderImpl read;
    return read.getExpandedLocationCode(locationCode, nodeNumber,
                                        fruLocationCode);
}

} // namespace manager
} // namespace vpd
} // namespace openpower
