#include "config.h"

#include "vpd_keyword_editor.hpp"

#include "editor_impl.hpp"
#include "parser.hpp"

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
    ServerObject<EditorIface>(bus, objPath),
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
        std::cerr << e.what() << "\n";
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
        }
    }
}

void VPDKeywordEditor::writeKeyword(const inventory::Path inventoryPath,
                                    const std::string recordName,
                                    const std::string keyword,
                                    const Binary value)
{
    try
    {
        if (frus.find(inventoryPath) == frus.end())
        {
            throw std::runtime_error("Inventory path not found");
        }

        inventory::Path vpdFilePath = frus.find(inventoryPath)->second.first;
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

            // update the cache, once data has been updated in file
            edit.updateCache();

#ifdef Kwd_Editor
            // if it is a motehrboard FRU need to check for location expansion
            if (frus.find(inventoryPath)->second.second)
            {
                if (recordName == "VCEN" &&
                    (keyword == "FC" || keyword == "SE"))
                {
                    edit.expandLocationCode("fcs");
                }
                else if (recordName == "VSYS" &&
                         (keyword == "TM" || keyword == "SE"))
                {
                    edit.expandLocationCode("mts");
                }
            }
#endif
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
