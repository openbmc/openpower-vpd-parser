#include "types.hpp"
#include "reader_impl.hpp"
#include "const.hpp"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <tuple>

using namespace openpower::vpd::manager::reader;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::constants;

class vpdManagerReaderTest : public ::testing::Test
{
  protected:
    nlohmann::json jsonFile;

    // map to hold the mapping of location code and inventory path
    LocationCodeMap fruLocationCode;
    
  public:
    // constructor
    vpdManagerReaderTest()
    {
        processJson();
    }
    
    void processJson();
};

void vpdManagerReaderTest::processJson()
{
    // read the json file and parse it
    std::ifstream json("vpd-manager-test/vpd_reader_test.json", std::ios::binary);
    
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
            fruLocationCode.emplace(
                itemEEPROM["extraInterfaces"][LOCATION_CODE_INF]["LocationCode"]
                    .get_ref<const nlohmann::json::string_t&>(),
                itemEEPROM["inventoryPath"]
                    .get_ref<const nlohmann::json::string_t&>());
        }
    }
}

TEST_F(vpdManagerReaderTest, getExpandedLocationCode_Invalid)
{
    std::string unexpandedLocationCode_Invalid = "Uabc-X0";
    uint16_t nodeNumber = 1;

    EXPECT_ANY_THROW
    ({
        ReaderImpl read;
        std::string expandedLocationCode = read.getExpandedLocationCode(unexpandedLocationCode_Invalid, nodeNumber, fruLocationCode);
    });
}

TEST_F(vpdManagerReaderTest, getFrusAtLocation_Invalid)
{
    std::string unexpandedLocationCode = "Uabc-X0";
    uint16_t nodeNumber = 1;

    EXPECT_ANY_THROW
    ({
        ReaderImpl read;
        std::string expandedLocationCode = read.getExpandedLocationCode(unexpandedLocationCode, nodeNumber, fruLocationCode);
    });
}

TEST_F(vpdManagerReaderTest, getFrusAtLocation_Valid)
{
    std::string LocationCode = "Ufcs-P0";
    uint16_t nodeNumber = 1;

    ReaderImpl read;
    ListOfPaths paths = read.getFrusAtLocation(LocationCode, nodeNumber, fruLocationCode);
    std::string expected = "/xyz/openbmc_project/inventory/system/chassis/motherboard";
    EXPECT_EQ(paths.at(0), expected);
}

TEST_F(vpdManagerReaderTest, getCollapsedLocationCode_invalid)
{
    //not starting from U
    std::string locationCode = "9105.22A.SIMP10R";

    ReaderImpl read;
    std::tuple<LocationCode, Node> res;
    
    EXPECT_ANY_THROW
    ({
        res = read.getCollapsedLocationCode(locationCode);
    });
}


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}