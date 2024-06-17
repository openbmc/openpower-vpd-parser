#include "reader_test.hpp"

#include "const.hpp"
#include "reader_impl.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <tuple>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace openpower::vpd::manager::reader;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::constants;

class vpdManagerReaderTest : public ::testing::Test
{
  protected:
    nlohmann::json jsonFile;

    // map to hold the mapping of location code and inventory path
    LocationCodeMap fruLocationCode;

    // dummy value of node number
    const uint8_t nodeNumber = 1;

  public:
    // constructor
    vpdManagerReaderTest()
    {
        processJson();
    }

    void processJson();
};

using namespace openpower::vpd;

void vpdManagerReaderTest::processJson()
{
    // read the json file and parse it
    std::ifstream json("vpd-manager-test/vpd_reader_test.json",
                       std::ios::binary);

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
                itemEEPROM["extraInterfaces"][IBM_LOCATION_CODE_INF]
                          ["LocationCode"]
                              .get_ref<const nlohmann::json::string_t&>(),
                itemEEPROM["inventoryPath"]
                    .get_ref<const nlohmann::json::string_t&>());
        }
    }
}

TEST_F(vpdManagerReaderTest, isValidLocationCode_invalid)
{
    // No MTS or FCS in the collapsed location code
    std::string unexpandedLocationCode_Invalid = "Uabc-X0";

    MockUtilCalls uCalls;
    ReaderImpl read(uCalls);
    EXPECT_ANY_THROW({
        read.getExpandedLocationCode(unexpandedLocationCode_Invalid, nodeNumber,
                                     fruLocationCode);
    });

    // not starting with U
    unexpandedLocationCode_Invalid = "Mabc-X0";
    EXPECT_ANY_THROW({
        read.getExpandedLocationCode(unexpandedLocationCode_Invalid, nodeNumber,
                                     fruLocationCode);
    });
}

TEST_F(vpdManagerReaderTest, getExpandedLocationCode_Invalid)
{
    std::string unexpandedLocationCode_Invalid = "Uabc-X0";

    MockUtilCalls uCalls;
    ReaderImpl read(uCalls);
    EXPECT_ANY_THROW({
        read.getExpandedLocationCode(unexpandedLocationCode_Invalid, nodeNumber,
                                     fruLocationCode);
    });
}

TEST_F(vpdManagerReaderTest, getExpandedLocationCode_Valid)
{
    // mock the call to read bus property
    MockUtilCalls uCalls;
    EXPECT_CALL(uCalls, readBusProperty(SYSTEM_OBJECT, IBM_LOCATION_CODE_INF,
                                        "LocationCode"))
        .Times(1)
        .WillOnce(testing::Return("U78DA.ND1.1234567-P0"));

    std::string unexpandedLocationCode = "Ufcs-P0";
    ReaderImpl read(uCalls);
    std::string res = read.getExpandedLocationCode(unexpandedLocationCode,
                                                   nodeNumber, fruLocationCode);

    EXPECT_EQ(res, "U78DA.ND1.1234567-P0");
}

TEST_F(vpdManagerReaderTest, getFrusAtLocation_Invalid)
{
    // invalid location code
    std::string unexpandedLocationCode = "Uabc-X0";

    MockUtilCalls uCalls;
    ReaderImpl read(uCalls);
    EXPECT_ANY_THROW({
        read.getFrusAtLocation(unexpandedLocationCode, nodeNumber,
                               fruLocationCode);
    });

    // map to hold the mapping of location code and inventory path, empty in
    // this case
    LocationCodeMap mapLocationCode;
    unexpandedLocationCode = "Ufcs-P0";
    EXPECT_ANY_THROW({
        read.getFrusAtLocation(unexpandedLocationCode, nodeNumber,
                               mapLocationCode);
    });
}

TEST_F(vpdManagerReaderTest, getFrusAtLocation_Valid)
{
    std::string LocationCode = "Ufcs-P0";

    MockUtilCalls uCalls;
    ReaderImpl read(uCalls);
    ListOfPaths paths = read.getFrusAtLocation(LocationCode, nodeNumber,
                                               fruLocationCode);
    std::string expected =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard";
    EXPECT_EQ(paths.at(0), expected);
}

TEST_F(vpdManagerReaderTest, getFRUsByExpandedLocationCode_invalid)
{
    // not starting from U
    std::string locationCode = "9105.22A.SIMP10R";

    MockUtilCalls uCalls;
    ReaderImpl read(uCalls);
    ListOfPaths paths;
    EXPECT_ANY_THROW({
        paths = read.getFRUsByExpandedLocationCode(locationCode,
                                                   fruLocationCode);
    });

    // unused variable warning
    (void)paths;

    // length is les sthan 17 for expanded location code
    locationCode = "U9105.22A.SIMP10";
    EXPECT_ANY_THROW({
        paths = read.getFRUsByExpandedLocationCode(locationCode,
                                                   fruLocationCode);
    });

    // Invalid location code. No "."
    locationCode = "U78DAND11234567-P0";

    // Mock readBUsproperty call
    EXPECT_CALL(uCalls,
                readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VCEN", "FC"))
        .Times(1)
        .WillOnce(
            testing::Return("78DAPQRS")); // return a dummy value for FC keyword

    // unused variable warning
    (void)paths;
    EXPECT_ANY_THROW({
        paths = read.getFRUsByExpandedLocationCode(locationCode,
                                                   fruLocationCode);
    });
}

TEST_F(vpdManagerReaderTest, getFRUsByExpandedLocationCode_Valid_FC)
{
    // valid location code with FC kwd.
    std::string validLocationCode = "U78DA.ND1.1234567-P0";

    // Mock readBUsproperty call
    MockUtilCalls uCalls;
    EXPECT_CALL(uCalls,
                readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VCEN", "FC"))
        .WillRepeatedly(
            testing::Return("78DAPQRS")); // return a dummy value for FC keyword

    ReaderImpl read(uCalls);
    ListOfPaths paths = read.getFRUsByExpandedLocationCode(validLocationCode,
                                                           fruLocationCode);

    std::string expected =
        "/xyz/openbmc_project/inventory/system/chassis/motherboard";
    EXPECT_EQ(paths.at(0), expected);
}

TEST_F(vpdManagerReaderTest, getFRUsByExpandedLocationCode_Valid_TM)
{
    // valid location code with TM kwd.
    std::string validLocationCode = "U9105.22A.SIMP10R";

    // Mock readBUsproperty call
    MockUtilCalls uCalls;
    EXPECT_CALL(uCalls,
                readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VCEN", "FC"))
        .Times(1)
        .WillOnce(
            testing::Return("78DAPQRS")); // return a dummy value for FC keyword

    EXPECT_CALL(uCalls,
                readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VSYS", "TM"))
        .Times(1)
        .WillOnce(
            testing::Return("9105PQRS")); // return a dummy value for TM keyword

    ReaderImpl read(uCalls);
    ListOfPaths paths = read.getFRUsByExpandedLocationCode(validLocationCode,
                                                           fruLocationCode);

    std::string expected = "/xyz/openbmc_project/inventory/system";
    EXPECT_EQ(paths.at(0), expected);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
