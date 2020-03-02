#include "config.h"

#include "manager.hpp"

#include "const.hpp"
#include "parser.hpp"

using namespace openpower::vpd::constants;
using namespace openpower::vpd::manager::editor;

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
            frus.emplace(itemEEPROM["inventoryPath"]
                             .get_ref<const nlohmann::json::string_t&>(),
                         itemFRUS.key());
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

        inventory::Path vpdFilePath = frus.find(path)->second;
        std::ifstream vpdStream(vpdFilePath, std::ios::binary);
        if (!vpdStream)
        {
            throw std::runtime_error("file not found");
        }

        Byte data;
        vpdStream.seekg(IPZ_DATA_START, std::ios::beg);
        vpdStream.get(*(reinterpret_cast<char*>(&data)));

        // implies it is IPZ VPD
        if (data == KW_VAL_PAIR_START_TAG)
        {
            Binary vpdHeader(lengths::VHDR_RECORD_LENGTH +
                             lengths::VHDR_ECC_LENGTH);
            vpdStream.seekg(0);
            vpdStream.read(reinterpret_cast<char*>(vpdHeader.data()),
                           vpdHeader.capacity());

            // check if header is valid
            openpower::vpd::keyword::editor::processHeader(
                std::move(vpdHeader));

            // instantiate editor class to update the data
            EditorImpl edit(vpdFilePath, jsonFile, recordName, keyword);
            edit.updateKeyword(value);

            return;
        }
        throw std::runtime_error("Invalid VPD file type");
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

std::vector<sdbusplus::message::object_path>
    Manager::getFRUsByUnexpandedLocationCode(const std::string locationCode,
                                             const uint16_t nodeNumber)
{
    // implement the interface
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
