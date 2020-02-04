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

constexpr int IPZ_DATA_START = 11;
constexpr int KW_VPD_DATA_START = 0;
constexpr uint8_t KW_VPD_START_TAG = 0x82;
constexpr uint8_t KW_VAL_PAIR_START_TAG = 0x84;

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

inventory::Path
    VPDKeywordEditor::processJSON(const inventory::Path inventoryPath)
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
    for (auto itemFRUS : groupFRUS.items())
    {
        std::vector<nlohmann::json> groupEEPROM = itemFRUS.value();
        for (auto itemEEPROM : groupEEPROM)
        {
            nlohmann::json individualFRU = itemEEPROM;
            if (individualFRU.find("inventoryPath").value() == inventoryPath)
            {
                return itemFRUS.key();
            }
        }
    }
    throw std::runtime_error("Inventory path is not found");
}

vpdType VPDKeywordEditor::vpdTypeCheck(const Binary& vpd)
{
    if (vpd[IPZ_DATA_START] == KW_VAL_PAIR_START_TAG)
    {
        // IPZ VPD FORMAT
        return vpdType::IPZ_VPD;
    }
    else if (vpd[KW_VPD_DATA_START] == KW_VPD_START_TAG)
    {
        // KEYWORD VPD FORMAT
        return vpdType::KWD_VPD;
    }

    return INVALID_VPD_FORMAT;
}

void VPDKeywordEditor::writeKeyword(const inventory::Path inventoryPath,
                                    const std::string recordName,
                                    const std::string keyword, Binary value)
{
    // process the json to get path to VPD file
    inventory::Path vpdFilePath = processJSON(inventoryPath);

    std::ifstream vpdFile(vpdFilePath, std::ios::binary);
    if (!vpdFile)
    {
        throw std::runtime_error("file not found");
    }

    Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
               std::istreambuf_iterator<char>());

    // check for VPD type
    vpdType type = vpdTypeCheck(vpd);
    if (!type)
    {
        throw std::runtime_error("Invalid VPD file type");
    }

    // instantiate editor class to update the data
    Editor edit(vpd);

    if (type == IPZ_VPD)
    {
        RecordOffset ptOffset;

        // parse vpd to validate Header and check TOC for PT record
        std::size_t ptLength =
            openpower::vpd::keyword::editor::processHeaderAndTOC(std::move(vpd),
                                                                 ptOffset);
        edit.updateKeyword(ptOffset, ptLength, recordName, keyword, value);
    }
}
} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
