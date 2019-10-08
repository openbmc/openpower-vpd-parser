#include "keyword_vpd_parser.hpp"
#include "keyword_vpd_types.hpp"

#include <exception>

#include <gtest/gtest.h>
using namespace vpd::keyword::parser;

TEST(KwVpdParser, GoodTestCase)
{
    Binary keywordVpdVector = {0x82, 0x03, 0x00, 0x01, 0x02, 0x03, 0x84, 0xA,
                               0x00, 0x41, 0x42, 0x02, 0x01, 0x02, 0x43, 0x44,
                               0x02, 0x03, 0x04, 0x79, 0x5A, 0x78};

    KeywordVpdParser parserObj1(move(keywordVpdVector));

    KeywordVpdMap map1 = {{"AB", {0x01, 0x02}}, {"CD", {0x03, 0x04}}};

    auto map2 = parserObj1.parseKwVpd();
    ASSERT_EQ(1, map1 == map2);

    keywordVpdVector = {
        0x82, 0x10, 0x00, 0x56, 0x53, 0x42, 0x50, 0x44, 0x30, 0x34, 0x4d, 0x30,
        0x20, 0x30, 0x36, 0x4,  0x53, 0x41, 0x53, 0x84, 0x9b, 0x00, 0x46, 0x4e,
        0x08, 0x20, 0x30, 0x31, 0x4b, 0x55, 0x37, 0x32, 0x34, 0x50, 0x4e, 0x07,
        0x30, 0x31, 0x4b, 0x55, 0x37, 0x32, 0x34, 0x53, 0x4e, 0x0c, 0x59, 0x48,
        0x33, 0x30, 0x42, 0x47, 0x37, 0x38, 0x42, 0x30, 0x31, 0x34, 0x43, 0x43,
        0x04, 0x32, 0x44, 0x33, 0x37, 0x43, 0x45, 0x01, 0x31, 0x56, 0x5a, 0x02,
        0x30, 0x33, 0x4d, 0x46, 0x02, 0x00, 0x10, 0x53, 0x4d, 0x40, 0x82, 0x50,
        0x32, 0x2d, 0x44, 0x34, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x32, 0x53,
        0x53, 0x43, 0x81, 0x50, 0x32, 0x2d, 0x44, 0x35, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x32, 0x53, 0x53, 0x43, 0x80, 0x50, 0x32, 0x2d, 0x44, 0x37,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x32, 0x53, 0x53, 0x43, 0x83, 0x50,
        0x32, 0x2d, 0x44, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x32, 0x53,
        0x53, 0x43, 0x46, 0x4c, 0x05, 0x50, 0x32, 0x20, 0x20, 0x20, 0x42, 0x32,
        0x10, 0x50, 0x05, 0x07, 0x60, 0x73, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x57, 0x49, 0x01, 0x00, 0x79, 0xb0, 0x78};

    KeywordVpdParser parserObj2(move(keywordVpdVector));
    map1 = {{"WI", {0x00}},
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

    map2 = parserObj2.parseKwVpd();
    ASSERT_EQ(1, map1 == map2);
}

TEST(KwVpdParser, BadTestCase)
{
    // Blank Vpd data
    Binary keywordVpdVector;
    KeywordVpdParser parserObj1(move(keywordVpdVector));
    EXPECT_THROW(parserObj1.parseKwVpd(), std::runtime_error);

    // Invalid Large resource type Identifier String - corrupted at index[0]
    keywordVpdVector = {0x83, 0x03, 0x00, 0x01, 0x02, 0x03, 0x84, 0xA,
                        0x00, 0x41, 0x42, 0x02, 0x01, 0x02, 0x43, 0x44,
                        0x02, 0x03, 0x04, 0x79, 0x5A, 0x78};
    KeywordVpdParser parserObj2(move(keywordVpdVector));
    EXPECT_THROW(parserObj2.parseKwVpd(), std::runtime_error);

    // Invalid Large resource type Vendor Defined - corrupted at index[6]
    keywordVpdVector = {0x82, 0x03, 0x00, 0x01, 0x02, 0x03, 0x85, 0xA,
                        0x00, 0x41, 0x42, 0x02, 0x01, 0x02, 0x43, 0x44,
                        0x02, 0x03, 0x04, 0x79, 0x5A, 0x78};
    KeywordVpdParser parserObj3(move(keywordVpdVector));
    EXPECT_THROW(parserObj3.parseKwVpd(), std::runtime_error);

    // Badly formed keyword VPD data - corrupted at index[7]
    keywordVpdVector = {0x82, 0x03, 0x00, 0x01, 0x02, 0x03, 0x84, 0x00,
                        0x00, 0x41, 0x42, 0x02, 0x01, 0x02, 0x43, 0x44,
                        0x02, 0x03, 0x04, 0x79, 0x5A, 0x78};
    KeywordVpdParser parserObj4(move(keywordVpdVector));
    EXPECT_THROW(parserObj4.parseKwVpd(), std::runtime_error);

    // Invalid Small resource type End - corrupted at index[19]
    keywordVpdVector = {0x82, 0x03, 0x00, 0x01, 0x02, 0x03, 0x84, 0xA,
                        0x00, 0x41, 0x42, 0x02, 0x01, 0x02, 0x43, 0x44,
                        0x02, 0x03, 0x04, 0x70, 0x5A, 0x78};
    KeywordVpdParser parserObj5(move(keywordVpdVector));
    EXPECT_THROW(parserObj5.parseKwVpd(), std::runtime_error);

    // Invalid Check sum - corrupted at index[20]
    keywordVpdVector = {0x82, 0x03, 0x00, 0x01, 0x02, 0x03, 0x84, 0xA,
                        0x00, 0x41, 0x42, 0x02, 0x01, 0x02, 0x43, 0x44,
                        0x02, 0x03, 0x04, 0x79, 0x5B, 0x78};
    KeywordVpdParser parserObj6(move(keywordVpdVector));
    EXPECT_THROW(parserObj6.parseKwVpd(), std::runtime_error);

    // Invalid Small resource type Last End Of Data - corrupted at index[21]
    keywordVpdVector = {0x82, 0x03, 0x00, 0x01, 0x02, 0x03, 0x84, 0xA,
                        0x00, 0x41, 0x42, 0x02, 0x01, 0x02, 0x43, 0x44,
                        0x02, 0x03, 0x04, 0x79, 0x5A, 0x76};
    KeywordVpdParser parserObj7(move(keywordVpdVector));
    EXPECT_THROW(parserObj7.parseKwVpd(), std::runtime_error);

    // Iterator Out of Bound - size is larger than the actual size - corrupted
    // at index[11]
    keywordVpdVector = {0x82, 0x03, 0x00, 0x01, 0x02, 0x03, 0x84, 0xA,
                        0x00, 0x41, 0x42, 0xB,  0x01, 0x02, 0x43, 0x44,
                        0x02, 0x03, 0x04, 0x79, 0x5A, 0x78};
    KeywordVpdParser parserObj8(move(keywordVpdVector));
    EXPECT_THROW(parserObj8.parseKwVpd(), std::runtime_error);

    // Iterator Out of Bound - size is smaller than the actual size - corrupted
    // at index[11]
    keywordVpdVector = {0x82, 0x03, 0x00, 0x01, 0x02, 0x03, 0x84, 0xA,
                        0x00, 0x41, 0x42, 0x01, 0x01, 0x02, 0x43, 0x44,
                        0x02, 0x03, 0x04, 0x79, 0x5A, 0x78};
    KeywordVpdParser parserObj9(move(keywordVpdVector));
    EXPECT_THROW(parserObj9.parseKwVpd(), std::runtime_error);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
