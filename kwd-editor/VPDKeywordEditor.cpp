#include "config.h"

#include "VPDKeywordEditor.hpp"

#include "editor.hpp"
#include "parser.hpp"

#include <tuple>

using namespace std::literals::chrono_literals;
using namespace openpower::vpd::constants;
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
                                    const std::string keyword, Binary value)
{
    try
    {
        if (frus.find(inventoryPath) == frus.end())
        {
            throw std::runtime_error("Inventory path not found");
        }

        inventory::Path vpdFilePath = frus.find(inventoryPath)->second;
        std::ifstream vpdStream(vpdFilePath, std::ios::binary);
        if (!vpdStream)
        {
            throw std::runtime_error("file not found");
        }

        Binary vpdHeader(lengths::VHDR_HEADER_LENGTH +
                         lengths::VHDR_ECC_LENGTH);
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
            // parse vpd to validate Header and check TOC for PT record
            auto PTinfo = openpower::vpd::keyword::editor::processHeaderAndTOC(
                std::move(vpdHeader));

            // instantiate editor class to update the data
            Editor edit(vpdFilePath, jsonFile, recordName, keyword);
            edit.updateKeyword(PTinfo, value);

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
