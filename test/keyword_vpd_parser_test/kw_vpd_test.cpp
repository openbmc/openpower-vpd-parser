#include "keyword_vpd_parser.hpp"

#include <exception>
#include <fstream>

#include <gtest/gtest.h>

using namespace vpd::keyword::parser;

class KeywordVpdParserTest : public ::testing::Test
{
  protected:
    Binary keywordVpdVector;
    Binary bonoKwVpdVector;

    KeywordVpdParserTest()
    {
        // Open the kw VPD file in binary mode
        std::ifstream kwVpdFile("vpd.dat", std::ios::binary);

        // Read the content of the binary file into a vector
        keywordVpdVector.assign((std::istreambuf_iterator<char>(kwVpdFile)),
                                std::istreambuf_iterator<char>());
        // Open the BONO type kw VPD file in binary mode
        std::ifstream bonoKwVpdFile("bono.vpd", std::ios::binary);

        // Read the content of the binary file into a vector
        bonoKwVpdVector.assign((std::istreambuf_iterator<char>(bonoKwVpdFile)),
                               std::istreambuf_iterator<char>());
    }
};

TEST_F(KeywordVpdParserTest, GoodTestCase)
{
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    KeywordVpdMap map1 = {
        {"WI", {0x00}},
        {"FL", {0x50, 0x32, 0x20, 0x20, 0x20}},
        {"SM",
         {0x82, 0x50, 0x32, 0x2d, 0x44, 0x34, 0x20, 0x20, 0x20, 0x20, 0x20,
          0x20, 0x32, 0x53, 0x53, 0x43, 0x81, 0x50, 0x32, 0x2d, 0x44, 0x35,
          0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x32, 0x53, 0x53, 0x43, 0x80,
          0x50, 0x32, 0x2d, 0x44, 0x37, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
          0x32, 0x53, 0x53, 0x43, 0x83, 0x50, 0x32, 0x2d, 0x44, 0x38, 0x20,
          0x20, 0x20, 0x20, 0x20, 0x20, 0x32, 0x53, 0x53, 0x43}},
        {"B2",
         {0x50, 0x05, 0x07, 0x60, 0x73, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x01, 0x00}},
        {"MF", {0x00, 0x10}},
        {"VZ", {0x30, 0x33}},
        {"PN", {0x30, 0x31, 0x4b, 0x55, 0x37, 0x32, 0x34}},
        {"FN", {0x20, 0x30, 0x31, 0x4b, 0x55, 0x37, 0x32, 0x34}},
        {"CE", {0x31}},
        {"SN",
         {0x59, 0x48, 0x33, 0x30, 0x42, 0x47, 0x37, 0x38, 0x42, 0x30, 0x31,
          0x34}},
        {"CC", {0x32, 0x44, 0x33, 0x37}}};

    auto map2 = parserObj1.parseKwVpd();
    ASSERT_EQ(1, map1 == map2);

    // BONO TYPE VPD
    KeywordVpdParser parserObj2(std::move(bonoKwVpdVector));
    map1 = {{"B2",
             {0x50, 0x0, 0xb3, 0xe0, 0x90, 0x0, 0x2, 0x50, 0x0, 0x0, 0x0, 0x0,
              0x0, 0x0, 0x0, 0x0}},
            {"CC", {0x35, 0x39, 0x33, 0x42}},
            {"CT", {0x50, 0x37, 0x32, 0x0}},
            {"EC", {0x50, 0x34, 0x35, 0x35, 0x33, 0x37}},
            {"FN", {0x30, 0x32, 0x44, 0x45, 0x33, 0x36, 0x35}},
            {"PN", {0x30, 0x32, 0x44, 0x45, 0x33, 0x36, 0x36}},
            {"RV", {0xa1}},
            {"SI", {0x31, 0x30, 0x31, 0x34, 0x30, 0x36, 0x37, 0x34}},
            {"SN",
             {0x59, 0x4c, 0x35, 0x30, 0x48, 0x54, 0x39, 0x36, 0x4a, 0x30, 0x30,
              0x38}},
            {"Z4", {0x30}},
            {"Z5", {0x30}},
            {"Z6", {0x41, 0x31, 0x38, 0x30, 0x30, 0x32, 0x30, 0x30}}};

    map2 = parserObj2.parseKwVpd();
    ASSERT_EQ(1, map1 == map2);
}

TEST_F(KeywordVpdParserTest, InvKwVpdTag)
{
    // Invalid Large resource type Identifier String - corrupted at index[0]
    keywordVpdVector[0] = 0x83;
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);

    // For BONO type VPD
    bonoKwVpdVector[0] = 0x83;
    KeywordVpdParser parserObj2(std::move(bonoKwVpdVector));
    EXPECT_THROW(parserObj2.parseKwVpd(), std::runtime_error);
}

TEST_F(KeywordVpdParserTest, InvKwValTag)
{
    // Invalid Large resource type Vendor Defined - corrupted at index[19]
    keywordVpdVector[19] = 0x85;
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);

    // For BONO type VPD - corruputed at index[33]
    bonoKwVpdVector[33] = 0x91;
    KeywordVpdParser parserObj2(std::move(bonoKwVpdVector));
    EXPECT_THROW(parserObj2.parseKwVpd(), std::runtime_error);
}

TEST_F(KeywordVpdParserTest, InvKwValSize)
{
    // Badly formed keyword VPD data - corrupted at index[20]
    keywordVpdVector[20] = 0x00;
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);

    // For BONO type VPD - corruputed at index[34]
    bonoKwVpdVector[34] = 0x00;
    KeywordVpdParser parserObj2(std::move(bonoKwVpdVector));
    EXPECT_THROW(parserObj2.parseKwVpd(), std::runtime_error);
}

TEST_F(KeywordVpdParserTest, InvKwValEndTag)
{
    // Invalid Small resource type End - corrupted at index[177]
    keywordVpdVector[177] = 0x80;
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);
}

TEST_F(KeywordVpdParserTest, InvChecksum)
{
    // Invalid Check sum - corrupted at index[178]
    keywordVpdVector[178] = 0xb1;
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);
}

TEST_F(KeywordVpdParserTest, InvKwVpdEndTag)
{
    // Invalid Small resource type Last End Of Data - corrupted at index[179]
    keywordVpdVector[179] = 0x79;
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);

    // For BONO type VPD - corrupted at index[147]
    bonoKwVpdVector[147] = 0x79;
    KeywordVpdParser parserObj2(std::move(bonoKwVpdVector));
    EXPECT_THROW(parserObj2.parseKwVpd(), std::runtime_error);
}

TEST_F(KeywordVpdParserTest, OutOfBoundGreaterSize)
{
    // Iterator Out of Bound - size is larger than the actual size - corrupted
    // at index[24]
    keywordVpdVector[24] = 0x32;
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);

    // For BONO type VPD - corrupted at index[38]
    bonoKwVpdVector[38] = 0x4D;
    KeywordVpdParser parserObj2(std::move(bonoKwVpdVector));
    EXPECT_THROW(parserObj2.parseKwVpd(), std::runtime_error);
}

TEST_F(KeywordVpdParserTest, OutOfBoundLesserSize)
{
    // Iterator Out of Bound - size is smaller than the actual size - corrupted
    // at index[24]
    keywordVpdVector[24] = 0x03;
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);

    // For BONO type VPD - corrupted at index[38]
    bonoKwVpdVector[38] = 0x04;
    KeywordVpdParser parserObj2(std::move(bonoKwVpdVector));
    EXPECT_THROW(parserObj2.parseKwVpd(), std::runtime_error);
}

TEST_F(KeywordVpdParserTest, BlankVpd)
{
    // Blank Kw Vpd
    keywordVpdVector.clear();
    KeywordVpdParser parserObj1(std::move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);

    // Blank Bono Type Vpd
    bonoKwVpdVector.clear();
    KeywordVpdParser parserObj2(std::move(bonoKwVpdVector));
    EXPECT_THROW(parserObj2.parseKwVpd(), std::runtime_error);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
