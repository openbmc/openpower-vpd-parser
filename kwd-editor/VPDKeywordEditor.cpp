#include "config.h"

#include "VPDKeywordEditor.hpp"

#include "parser.hpp"

#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
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

void VPDKeywordEditor::processAndUpdateCI(const std::string recName,
                                          const std::string kwdName)
{
    for (auto& commonInterface : jsonFile["commonInterfaces"].items())
    {
        for (auto& ci_propertyList : commonInterface.value().items())
        {
            if (((ci_propertyList.value().get<nlohmann::json>())["recordName"]
                     .get<std::string>() == recName) &&
                ((ci_propertyList.value().get<nlohmann::json>())["keywordName"]
                     .get<std::string>() == kwdName))
            {
                // implement call to dbus to update properties
            }
        }
    }
}

void VPDKeywordEditor::updateCache(const inventory::Path vpdFilePath,
                                   const std::string recName,
                                   const std::string kwdName)
{
    std::vector<nlohmann::json> groupEEPROM =
        jsonFile["frus"][vpdFilePath].get<std::vector<nlohmann::json>>();

    // iterate through all the inventories for this file path
    for (auto& singleEEPROM : groupEEPROM)
    {
        // by default inherit property is true
        bool isInherit = true;

        if (singleEEPROM.find("inherit") != singleEEPROM.end())
        {
            isInherit = singleEEPROM["inherit"].get<bool>();
        }

        if (isInherit)
        {
            processAndUpdateCI(recName, kwdName);
        }

        // process extra interfaces
        for (auto& extraInterface : singleEEPROM["extraInterfaces"].items())
        {
            if (extraInterface.value() != NULL)
            {
                for (auto& ei_propertyList : extraInterface.value().items())
                {
                    if (((ei_propertyList.value()
                              .get<nlohmann::json>())["recordName"]
                             .get<std::string>() == recName) &&
                        ((ei_propertyList.value()
                              .get<nlohmann::json>())["keywordName"]
                             .get<std::string>() == kwdName))
                    {
                        // implement dbus calls to update properties
                    }
                }
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

            // update the cache once data has been updated
            updateCache(vpdFilePath, recordName, keyword);
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
