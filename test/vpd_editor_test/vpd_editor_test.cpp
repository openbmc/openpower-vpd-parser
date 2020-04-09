#include "parser.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <vector>

#include <gtest/gtest.h>
//#include "types.hpp"
#include "editor_impl.hpp"

using namespace openpower::vpd;
using namespace openpower::vpd::keyword::editor;

class vpdEditorTest : public ::testing::Test
{

  protected:
    // Binary vpdFileBeforeUpdate;
    Binary requiredRecordOutput;
    nlohmann::json jsonFile;

    // these values are hardcoded as we have to compare the data with the
    // updated file reading complete file will be inefficient and as test cases
    // are run on dummy file The kwdOffset, its size and eccoffset is constant in
    // all the cases
    const uint16_t kwdOffset = 276;
    const uint16_t recECCoffset = 2357;
    const uint16_t recECCLength = 33;
    const uint16_t kwdSize = 12;

  public:
    // constructor
    vpdEditorTest()
    {
        // read the json file and parse it
        std::ifstream json("vpd.json", std::ios::binary);
        jsonFile = nlohmann::json::parse(json);
    }
};

TEST_F(vpdEditorTest, InvalidKwdTest)
{
    std::string result{};
    try
    {
        // length 12 is used because the kwd we are updating is of length 12
        Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                            'D', 'A', 'T', 'A', 'O', 'K'};

        // edit the data
        EditorImpl edit("vpdFile_inv_kwd.dat", jsonFile, "VINI",
                        "Sn"); //"Sn is invalid keyword"
        edit.updateKeyword(dataToUodate);
    }
    catch (const std::exception& e)
    {
        result = e.what();
    }
    ASSERT_STREQ("Keyword not found", result.c_str());
}

TEST_F(vpdEditorTest, InvalidHeader)
{
    std::string result{};
    try
    {
        // length 12 is used because the kwd we are updating is of length 12
        Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                            'D', 'A', 'T', 'A', 'O', 'K'};

        std::ifstream vpdStream("invalidHeaderFile.dat", std::ios::binary);
        Binary vpdHeader(lengths::VHDR_RECORD_LENGTH +
                         lengths::VHDR_ECC_LENGTH);
        vpdStream.seekg(0);
        vpdStream.read(reinterpret_cast<char*>(vpdHeader.data()),
                       vpdHeader.capacity());

        // check if header is valid
        openpower::vpd::keyword::editor::processHeader(std::move(vpdHeader));

        // edit the data
        EditorImpl edit("vpdFile.dat", jsonFile, "VINI", "SN");
        edit.updateKeyword(dataToUodate);
    }
    catch (const std::exception& e)
    {
        result = e.what();
    }
    ASSERT_STREQ("VHDR record not found", result.c_str());
}

TEST_F(vpdEditorTest, InvalidRecordTest)
{
    std::string result{};
    try
    {
        // length 12 is used because the kwd we are updating is of length 12
        Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                            'D', 'A', 'T', 'A', 'O', 'K'};

        // edit the data
        EditorImpl edit("vpdFile_inv_rec.dat", jsonFile, "VIN",
                        "SN"); //"VIN is invalid record"
        edit.updateKeyword(dataToUodate);
    }
    catch (const std::exception& e)
    {
        result = e.what();
    }
    ASSERT_STREQ("Record not found", result.c_str());
}

TEST_F(vpdEditorTest, DataLengthLessThanKwdSize)
{
    Binary dataToUodate{'U', 'P', 'D', 'A', 'T', 'E'};
    Binary resultdata{'U', 'P', 'D', 'A', 'T', 'E',
                      '0', '1', '0', '0', '0', '0'};

    // edit the data
    EditorImpl edit("vpdFile.dat", jsonFile, "VINI", "SN");
    edit.updateKeyword(dataToUodate);

    // read the updated data from the file
    std::ifstream vpd("vpdFile.dat", std::ios::binary);

    // read the updated record data from the modified file
    Binary updatedRecordData(kwdSize);
    vpd.seekg(kwdOffset, std::ios::beg);
    vpd.read(reinterpret_cast<char*>(updatedRecordData.data()), kwdSize);

    for (auto i : updatedRecordData)
    {
        std::cout << i << std::endl;
    }
    // TODO: update this line as data updated is less in length
    bool isEqual = std::equal(updatedRecordData.cbegin(),
                              updatedRecordData.cend(), resultdata.cbegin());

    // true as dota should be updated in this case
    ASSERT_EQ(true, isEqual);
}

TEST_F(vpdEditorTest, Allvalid_kwdUpdatedSuccessfully)
{
    // length 12 is used because the kwd we are updating is of length 12
    Binary dataToUodate{'M', 'O', 'D', 'I', 'F', 'Y',
                        'D', 'A', 'T', 'A', 'O', 'K'};

    // in this case header is valid,hence data should be updated
    std::ifstream vpdStream("vpdFile.dat", std::ios::binary);
    Binary vpdHeader(lengths::VHDR_RECORD_LENGTH + lengths::VHDR_ECC_LENGTH);
    vpdStream.seekg(0);
    vpdStream.read(reinterpret_cast<char*>(vpdHeader.data()),
                   vpdHeader.capacity());

    // check if header is valid
    openpower::vpd::keyword::editor::processHeader(std::move(vpdHeader));

    // edit the data
    EditorImpl edit("vpdFile.dat", jsonFile, "VINI", "SN");
    edit.updateKeyword(dataToUodate);

    // read the data from the file after update
    std::ifstream vpd("vpdFile.dat", std::ios::binary);
    Binary updatedRecordData(kwdSize);
    vpd.seekg(kwdOffset, std::ios::beg);
    vpd.read(reinterpret_cast<char*>(updatedRecordData.data()), kwdSize);

    // updated data should be equal to the data we wanted to update in this case
    // as the length of data passed was equal to the size of kwdDataSize
    bool isEqual = std::equal(updatedRecordData.cbegin(),
                              updatedRecordData.cend(), dataToUodate.cbegin());

    // data should be updated as all the conditions are valid in this case
    ASSERT_EQ(true, isEqual);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
