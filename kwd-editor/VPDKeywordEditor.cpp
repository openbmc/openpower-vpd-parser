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
constexpr uint8_t KW_VAL_PAIR_START_TAG = 0x84;
constexpr uint8_t RECORD_END_TAG = 0x78;

enum length
{
    VHDR_HEADER_LENGTH = 44,
    VHDR_ECC_LENGTH = 11,
};

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
            if (individualFRU.find("inventoryPath") != individualFRU.end())
            {
                inventory::Path path =
                    individualFRU.find("inventoryPath").value();
                frus.insert({path, itemFRUS.key()});
            }
        }
    }
}

void VPDKeywordEditor::writeKeyword(const inventory::Path inventoryPath,
                                    const std::string recordName,
                                    const std::string keyword, Binary value)
{
    try
    {
        // process the json to get path to VPD file
        inventory::Path vpdFilePath = processJSON(inventoryPath);

        std::ifstream vpdStream(vpdFilePath, std::ios::binary);
        if (!vpdStream)
        {
            throw std::runtime_error("file not found");
        }

        Binary vpdHeader(length::VHDR_HEADER_LENGTH + length::VHDR_ECC_LENGTH);
        vpdStream.read(reinterpret_cast<char*>(vpdHeader.data()),
                       vpdHeader.capacity());

        Binary vpdTOC;

        Byte data;
        while (vpdStream.get(*(reinterpret_cast<char*>(&data))))
        {
            vpdTOC.push_back(data);
            if (data == RECORD_END_TAG)
            {
                // record has been fully read
                break;
            }
        }

        vpdHeader.insert(vpdHeader.end(), vpdTOC.begin(), vpdTOC.end());

        vpdStream.seekg(IPZ_DATA_START, std::ios::beg);
        vpdStream.get(*(reinterpret_cast<char*>(&data)));

        // imples it is IPZ VPD
        if (data == KW_VAL_PAIR_START_TAG)
        {
            RecordOffset ptOffset;

            // parse vpd to validate Header and check TOC for PT record
            std::size_t ptLength =
                openpower::vpd::keyword::editor::processHeaderAndTOC(
                    std::move(vpdHeader), ptOffset);

            // instantiate editor class to update the data
            Editor edit(vpdFilePath, recordName, keyword);
            edit.updateKeyword(ptOffset, ptLength, value);

            return;
        }
        throw std::runtime_error("Invalid VPD file type");
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}
} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
