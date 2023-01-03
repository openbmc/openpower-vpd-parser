#include "editor_impl.hpp"
#include "ipz_parser.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <vector>

#include <gtest/gtest.h>

using namespace openpower::vpd;
using namespace openpower::vpd::manager;
using namespace openpower::vpd::types;
using namespace openpower::vpd::constants;

class vpdManagerEditorTest : public ::testing::Test
{
  protected:
    Binary vpd;

    nlohmann::json jsonFile;

    // map to hold the mapping of location code and inventory path
    LocationCodeMap fruLocationCode;

  public:
    // constructor
    vpdManagerEditorTest()
    {
        processJson();
    }

    void processJson();
    void readFile(std::string pathToFile);
};

void vpdManagerEditorTest::readFile(std::string pathToFile)
{
    // read the json file and parse it
    std::ifstream vpdFile(pathToFile, std::ios::binary);

    if (!vpdFile)
    {
        throw std::runtime_error("json file not found");
    }

    vpd.assign((std::istreambuf_iterator<char>(vpdFile)),
               std::istreambuf_iterator<char>());
}

void vpdManagerEditorTest::processJson()
{
    // read the json file and parse it
    std::ifstream json("vpd-manager-test/vpd_editor_test.json",
                       std::ios::binary);

    if (!json)
    {
        throw std::runtime_error("json file not found");
    }

    jsonFile = nlohmann::json::parse(json);

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

TEST_F(vpdManagerEditorTest, InvalidFile)
{
    Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                        'D', 'A', 'T', 'A', 'O', 'K'};

    Binary emptyVpdFile;
    try
    {
        // Invalid kwd name
        EditorImpl edit("VINI", "SN", std::move(emptyVpdFile));
        edit.updateKeyword(dataToUodate, 0, true);
    }
    catch (const std::exception& e)
    {
        EXPECT_EQ(std::string(e.what()), std::string("Invalid File"));
    }
}

TEST_F(vpdManagerEditorTest, InvalidHeader)
{
    Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                        'D', 'A', 'T', 'A', 'O', 'K'};

    readFile("vpd-manager-test/invalidHeaderFile.dat");
    try
    {
        // the path is dummy
        EditorImpl edit("VINI", "SN", std::move(vpd));
        edit.updateKeyword(dataToUodate, 0, true);
    }
    catch (const std::exception& e)
    {
        EXPECT_EQ(std::string(e.what()), std::string("VHDR record not found"));
    }
}

TEST_F(vpdManagerEditorTest, InvalidRecordName)
{
    Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                        'D', 'A', 'T', 'A', 'O', 'K'};

    readFile("vpd-manager-test/vpdFile.dat");

    try
    {
        // Invalid record name "VIN", path is dummy
        EditorImpl edit("VIN", "SN", std::move(vpd));
        edit.updateKeyword(dataToUodate, 0, true);
    }
    catch (const std::exception& e)
    {
        EXPECT_EQ(std::string(e.what()), std::string("Record not found"));
    }
}

TEST_F(vpdManagerEditorTest, InvalidKWdName)
{
    Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                        'D', 'A', 'T', 'A', 'O', 'K'};

    readFile("vpd-manager-test/vpdFile.dat");

    try
    {
        // All valid data
        EditorImpl edit("VINI", "Sn", std::move(vpd));
        edit.updateKeyword(dataToUodate, 0, true);
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_EQ(std::string(e.what()), std::string("Keyword not found"));
    }
}

TEST_F(vpdManagerEditorTest, UpdateKwd_Success)
{
    Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                        'D', 'A', 'T', 'A', 'O', 'K'};

    readFile("vpd-manager-test/vpdFile.dat");

    try
    {
        // All valid data, but can't update with dummy ECC code
        EditorImpl edit("VINI", "SN", std::move(vpd));
        edit.updateKeyword(dataToUodate, 0, true);
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_EQ(std::string(e.what()), std::string("Ecc update failed"));
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}