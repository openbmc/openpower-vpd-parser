#include <fstream>
#include <list>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

using namespace std;
using json = nlohmann::json;

json jsonSample =
    "{\"commonInterfaces\":{\"xyz.openbmc_project.Inventory.Decorator.Asset\":{\"PartNumber\":{\"keywordName\":\"PN\",\"recordName\":\"VINI\"},\"SerialNumber\":{\"keywordName\":\"SN\",\"recordName\":\"VINI\"}},\"xyz.openbmc_project.Inventory.Item\":{\"PrettyName\":{\"keywordName\":\"DR\",\"recordName\":\"VINI\"}}},\"frus\":{\"/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a080.i2c-bus/i2c-0/0-0051/0-00510/nvmem\":{\"extraInterfaces\":{\"xyz.openbmc_project.Inventory.Item.Tpm\":null},\"inventoryPath\":\"/system/chassis/motherboard/tpm_wilson\"},\"/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a400.i2c-bus/i2c-7/7-0050/7-00500/nvmem\":{\"extraInterfaces\":{\"xyz.openbmc_project.Inventory.Item.Panel\":null},\"inventoryPath\":\"/system/chassis/motherboard/base_op_panel_blyth\"},\"/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a400.i2c-bus/i2c-7/7-0051/7-00510/nvmem\":{\"extraInterfaces\":{\"xyz.openbmc_project.Inventory.Item.Panel\":null},\"inventoryPath\":\"/system/chassis/motherboard/lcd_op_panel_hill\"},\"/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a480.i2c-bus/i2c-8/8-0050/8-00500/nvmem\":{\"extraInterfaces\":{\"xyz.openbmc_project.Inventory.Item.Board.Motherboard\":null},\"inventoryPath\":\"/system/chassis/motherboard\"},\"/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a480.i2c-bus/i2c-8/8-0051/8-00510/nvmem\":{\"extraInterfaces\":{\"xyz.openbmc_project.Inventory.Item.Bmc\":null},\"inventoryPath\":\"/system/chassis/motherboard/ebmc_card_bmc\"},\"/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a500.i2c-bus/i2c-9/9-0050/9-00500/nvmem\":{\"extraInterfaces\":{\"xyz.openbmc_project.Inventory.Item.Vrm\":null},\"inventoryPath\":\"/system/chassis/motherboard/vdd_vrm0\"},\"/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a580.i2c-bus/i2c-10/10-0050/10-00500/nvmem\":{\"extraInterfaces\":{\"xyz.openbmc_project.Inventory.Item.Vrm\":null},\"inventoryPath\":\"/system/chassis/motherboard/vdd_vrm1\"}}}"_json;

list<string> supportedFrus{
    "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a480.i2c-bus/"
    "i2c-8/8-0050/8-00500/nvmem",
    "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a480.i2c-bus/"
    "i2c-8/8-0051/8-00510/nvmem",
    "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a080.i2c-bus/"
    "i2c-0/0-0051/0-00510/nvmem",
    "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a400.i2c-bus/"
    "i2c-7/7-0050/7-00500/nvmem",
    "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a400.i2c-bus/"
    "i2c-7/7-0051/7-00510/nvmem",
    "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a500.i2c-bus/"
    "i2c-9/9-0050/9-00500/nvmem",
    "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a580.i2c-bus/"
    "i2c-10/10-0050/10-00500/nvmem"};

TEST(verifyInventoryJSON, inventoryGoodPath)
{
    string file("/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/"
                "1e78a400.i2c-bus/i2c-7/7-0050/7-00500/nvmem");
    if(
    ifstream inventoryJson(
        "/usr/share/vpd/vpd_inventory.json")) // INVENTORY_JSON);
    {
    auto js = json::parse(inventoryJson);

    // TEST1: check the parsed json is same as we expect
    ASSERT_EQ(js, jsonSample);

    // TEST2: check "frus" defined in json
    if (js.find("frus") == js.end())
    {
        throw std::runtime_error("FRU is not defined in inventory JSON");
    }

    for (auto& eachFru : supportedFrus)
    {
        // TEST3: Check for all the FRU's path is defined in the inventory JSON
        if (js["frus"].find(eachFru) == js["frus"].end())
        {
            throw std::runtime_error("Device path missing in inventory JSON");
        }

        // TEST4: check for object is present for each FRU.
        if (js["frus"][eachFru].find("inventoryPath") ==
            js["frus"][eachFru].end())
        {
            throw std::runtime_error(
                "inventoryPath is Not defined for at least one fru.");
        }

        // TEST5: check for extraInterfaces is present in each FRU.
        if (js["frus"][eachFru].find("extraInterfaces") ==
            js["frus"][eachFru].end())
        {
            throw std::runtime_error(
                "extraInterfaces is Not defined for at least one fru.");
        }
    }

    // TEST6: check "commonInterfaces" defined in json
    if (js.find("commonInterfaces") == js.end())
    {
        throw std::runtime_error(
            "commonInterfaces Not defined in inventory JSON");
    }

    // TEST7: check decorator is defined
    if (js["commonInterfaces"].find(
            "xyz.openbmc_project.Inventory.Decorator.Asset") ==
        js["commonInterfaces"].end())
    {
        throw std::runtime_error("Decorator Not defined in the json");
    }

    // TEST8: check PartNumber is defined
    if (js["commonInterfaces"]["xyz.openbmc_project.Inventory.Decorator.Asset"]
            .find("PartNumber") ==
        js["commonInterfaces"]["xyz.openbmc_project.Inventory.Decorator.Asset"]
            .end())
    {
        throw std::runtime_error(
            "PartNumber not defined in commonInterfaces in json");
    }

    // TEST9: Read "recordName" & "keywordName".This is suppose to be VINI & PN
    // respectively.
    string rec =
        js["commonInterfaces"]["xyz.openbmc_project.Inventory.Decorator.Asset"]
          ["PartNumber"]["recordName"];
    string kw =
        js["commonInterfaces"]["xyz.openbmc_project.Inventory.Decorator.Asset"]
          ["PartNumber"]["keywordName"];

    ASSERT_EQ(rec, "VINI");
    ASSERT_EQ(kw, "PN");

    // TEST10: check SerialNumber is defined
    if (js["commonInterfaces"]["xyz.openbmc_project.Inventory.Decorator.Asset"]
            .find("SerialNumber") ==
        js["commonInterfaces"]["xyz.openbmc_project.Inventory.Decorator.Asset"]
            .end())
    {
        throw std::runtime_error(
            "SerialNumber not defined in commonInterfaces in json");
    }

    // TES11: Read "recordName" & "keywordName".This is suppose to be VINI & SN
    // respectively.
    rec =
        js["commonInterfaces"]["xyz.openbmc_project.Inventory.Decorator.Asset"]
          ["SerialNumber"]["recordName"];
    kw = js["commonInterfaces"]["xyz.openbmc_project.Inventory.Decorator.Asset"]
           ["SerialNumber"]["keywordName"];

    ASSERT_EQ(rec, "VINI");
    ASSERT_EQ(kw, "SN");

    // TEST12: check Inventory Item is defined
    if (js["commonInterfaces"].find("xyz.openbmc_project.Inventory.Item") ==
        js["commonInterfaces"].end())
    {
        throw std::runtime_error("Inventory Item Not defined in the json");
    }

    // TEST13: check PrettyName is defined
    if (js["commonInterfaces"]["xyz.openbmc_project.Inventory.Item"].find(
            "PrettyName") ==
        js["commonInterfaces"]["xyz.openbmc_project.Inventory.Item"].end())
    {
        throw std::runtime_error(
            "PrettyName not defined in commonInterfaces in json");
    }

    // TES14: Read "recordName" & "keywordName".This is suppose to be VINI & DR
    // respectively.
    rec = js["commonInterfaces"]["xyz.openbmc_project.Inventory.Item"]
            ["PrettyName"]["recordName"];
    kw = js["commonInterfaces"]["xyz.openbmc_project.Inventory.Item"]
           ["PrettyName"]["keywordName"];

    ASSERT_EQ(rec, "VINI");
    ASSERT_EQ(kw, "DR");
    }
    else
    {
        throw std::runtime_error("Json file does not exist in given path");
    }

}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
