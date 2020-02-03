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
    try
    {
        processJSON();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    while (true)
    {
        _bus.process_discard();

        // wait for event
        _bus.wait();
    }
}

void VPDKeywordEditor::processJSON()
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

    nlohmann::json groupFRUS = (jsonFile.find("frus")).value();
    for (auto itemFRUS : groupFRUS.items())
    {
        std::vector<nlohmann::json> groupEEPROM = itemFRUS.value();
        for (auto itemEEPROM : groupEEPROM)
        {
            frus.insert({itemEEPROM["inventoryPath"].get<std::string>(),
                         itemFRUS.key()});
        }
    }
}

void VPDKeywordEditor::writeKeyword(const inventory::Path inventoryPath,
                                    const std::string recordName,
                                    const std::string keyword,
                                    std::vector<uint8_t> value)
{
    // implement write functionality here
}

} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
