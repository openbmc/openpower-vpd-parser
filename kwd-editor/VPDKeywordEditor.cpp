#include "config.h"
#include "VPDKeywordEditor.hpp"
#include "parser.hpp"
#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

using namespace std::literals::chrono_literals;
namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{
VPDKeywordEditor::VPDKeywordEditor(sdbusplus::bus::bus&& bus,
                                   const char* busName, const char* objPath,
                                   const char* iFace) :
    ServerObject<kwdEditorIface>(bus, objPath),
    _bus(std::move(bus)), _manager(_bus, objPath)
{
    _bus.request_name(busName);
}

void VPDKeywordEditor::run()
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
            std::cerr << e.what() << std::endl;
        }
    }
}

inventory::vpdPath VPDKeywordEditor::processJSON(std::string inventoryPath)
{
    std::ifstream jsonFile(INVENTORY_JSON, std::ios::binary);

    if (!jsonFile)
    {
        throw std::runtime_error("json file not found");
    }

    nlohmann::json jfile = nlohmann::json::parse(jsonFile);
    if (jfile.find("frus") == jfile.end())
    {
        throw std::runtime_error("frus group not found in json");
    }

    nlohmann::json groupFRUS = (jfile.find("frus")).value();
    nlohmann::json::iterator itemFRUS;
    for (itemFRUS = groupFRUS.begin(); itemFRUS != groupFRUS.end(); ++itemFRUS)
    {
        std::vector<nlohmann::json> groupEEPROM = itemFRUS.value();
        std::vector<nlohmann::json>::iterator itemEEPROM;

        for (itemEEPROM = groupEEPROM.begin(); itemEEPROM != groupEEPROM.end();
             ++itemEEPROM)
        {
            nlohmann::json individualFRU = *itemEEPROM;
            if (individualFRU.find("inventoryPath").value() == inventoryPath)
            {
                return itemFRUS.key();
            }
        }
    }
    throw std::runtime_error("Inventory path is not found");
}

void VPDKeywordEditor::writeKeyword(std::string inventoryPath,
                                    std::string recordName, std::string keyword,
                                    std::vector<uint8_t> value)
{
    // process the json to get path to VPD file
    inventory::vpdPath vpdFilePath = processJSON(inventoryPath);
}
} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
