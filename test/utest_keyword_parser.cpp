#include "exceptions.hpp"
#include "keyword_vpd_parser.hpp"
#include "parser.hpp"
#include "types.hpp"

#include <cstdint>
#include <exception>
#include <fstream>

#include <gtest/gtest.h>

using namespace vpd;

TEST(KeywordVpdParserTest, GoodTestCase)
{
    types::KeywordVpdMap l_keywordMap{
        std::pair<std::string, types::BinaryVector>{"WI", {0x00}},
        std::pair<std::string, types::BinaryVector>{
            "FL", {0x50, 0x32, 0x20, 0x20, 0x20}},
        std::pair<std::string, types::BinaryVector>{
            "SM",
            {0x82, 0x50, 0x32, 0x2d, 0x44, 0x34, 0x20, 0x20, 0x20, 0x20, 0x20,
             0x20, 0x32, 0x53, 0x53, 0x43, 0x81, 0x50, 0x32, 0x2d, 0x44, 0x35,
             0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x32, 0x53, 0x53, 0x43, 0x80,
             0x50, 0x32, 0x2d, 0x44, 0x37, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
             0x32, 0x53, 0x53, 0x43, 0x83, 0x50, 0x32, 0x2d, 0x44, 0x38, 0x20,
             0x20, 0x20, 0x20, 0x20, 0x20, 0x32, 0x53, 0x53, 0x43}},
        std::pair<std::string, types::BinaryVector>{
            "B2",
            {0x50, 0x05, 0x07, 0x60, 0x73, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00,
             0x00, 0x00, 0x00, 0x01, 0x00}},
        std::pair<std::string, types::BinaryVector>{"MF", {0x00, 0x10}},
        std::pair<std::string, types::BinaryVector>{"VZ", {0x30, 0x33}},
        std::pair<std::string, types::BinaryVector>{
            "PN", {0x30, 0x31, 0x4b, 0x55, 0x37, 0x32, 0x34}},
        std::pair<std::string, types::BinaryVector>{
            "FN", {0x20, 0x30, 0x31, 0x4b, 0x55, 0x37, 0x32, 0x34}},
        std::pair<std::string, types::BinaryVector>{"CE", {0x31}},
        std::pair<std::string, types::BinaryVector>{"SN",
                                                    {0x59, 0x48, 0x33, 0x30,
                                                     0x42, 0x47, 0x37, 0x38,
                                                     0x42, 0x30, 0x31, 0x34}},
        std::pair<std::string, types::BinaryVector>{"CC",
                                                    {0x32, 0x44, 0x33, 0x37}}};

    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/keyword.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    ASSERT_EQ(1, std::get<types::KeywordVpdMap>(l_vpdParser.parse()) ==
                     l_keywordMap);
}

TEST(KeywordVpdParserTest, InvalidVpd)
{
    // Invalid large resource type identifier string - corrupted at index[0]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/keyword_corrupted_index_0.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), DataException);
}

TEST(KeywordVpdParserTest, InvalidStartTag)
{
    // Invalid large resource type vendor defined - corrupted at index[19]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/keyword_corrupted_index_19.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), DataException);
}

TEST(KeywordVpdParserTest, InvalidSize)
{
    // Badly formed keyword VPD data - corrupted at index[20]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/keyword_corrupted_index_20.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), DataException);
}

TEST(KeywordVpdParserTest, InvalidEndTag)
{
    // Invalid small resource type end - corrupted at index[177]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/keyword_corrupted_index_177.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), DataException);
}

TEST(KeywordVpdParserTest, InvalidChecksum)
{
    // Invalid check sum - corrupted at index[178]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/keyword_corrupted_index_178.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), DataException);
}

TEST(KeywordVpdParserTest, InvalidLastEndTag)
{
    // Invalid small resource type last end of data - corrupted at index[179]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/keyword_corrupted_index_179.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), DataException);
}

TEST(KeywordVpdParserTest, OutOfBoundGreaterSize)
{
    // Iterator out of bound - size is larger than the actual size - corrupted
    // at index[24]
    nlohmann::json l_json;
    std::string l_vpdFile(
        "vpd_files/keyword_corrupted_index_24_large_size.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), DataException);
}

TEST(KeywordVpdParserTest, OutOfBoundLesserSize)
{
    // Iterator out of bound - size is smaller than the actual size - corrupted
    // at index[24]
    nlohmann::json l_json;
    std::string l_vpdFile(
        "vpd_files/keyword_corrupted_index_24_small_size.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), DataException);
}

TEST(KeywordVpdParserTest, EmptyInput)
{
    // Blank keyword VPD
    types::BinaryVector emptyVector{};
    KeywordVpdParser l_keywordParser(std::move(emptyVector));

    EXPECT_THROW(l_keywordParser.parse(), std::exception);
}
