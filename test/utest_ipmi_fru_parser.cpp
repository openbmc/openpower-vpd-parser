#include "exceptions.hpp"
#include "ipmi_fru_parser.hpp"
#include "types.hpp"

#include <exception>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

using namespace vpd;

namespace
{

/**
 * @brief Helper function to compute 2's complement zero-sum checksum
 */
uint8_t calculateChecksum(const types::BinaryVector& i_vector, size_t i_start,
                          size_t i_length)
{
    uint8_t l_sum = 0;
    for (size_t l_idx = i_start; l_idx < i_start + i_length; ++l_idx)
    {
        l_sum = static_cast<uint8_t>(l_sum + i_vector[l_idx]);
    }
    return static_cast<uint8_t>(-l_sum);
}

/**
 * @brief Helper function to read binary VPD data from a file
 *
 * @param[in] i_filePath - Path to the binary VPD file
 * @return types::BinaryVector - Raw bytes read from file
 */
types::BinaryVector readVpdFile(const std::string& i_filePath)
{
    std::ifstream l_stream(i_filePath, std::ios::binary);
    if (!l_stream)
    {
        throw std::runtime_error("Failed to open VPD file: " + i_filePath);
    }

    return types::BinaryVector(std::istreambuf_iterator<char>(l_stream),
                               std::istreambuf_iterator<char>());
}

/**
 * @brief Helper to generate valid IPMI FRU binary buffer containing Common
 * Header and Product Info Area
 */
types::BinaryVector createSampleIpmiFruVpd()
{
    // Common Header: 8 bytes
    // Format Version = 0x01, Product Info Area offset = 1 (8 bytes), Checksum
    // at byte 7
    types::BinaryVector l_vpdVector(120, 0x00);

    // Common Header
    l_vpdVector[0] = 0x01; // Format Version
    l_vpdVector[1] = 0x00; // Internal Use Area Offset
    l_vpdVector[2] = 0x00; // Chassis Info Area Offset
    l_vpdVector[3] = 0x00; // Board Info Area Offset
    l_vpdVector[4] = 0x01; // Product Info Area Offset (8 bytes)
    l_vpdVector[5] = 0x00; // MultiRecord Area Offset
    l_vpdVector[6] = 0x00; // PAD
    l_vpdVector[7] = calculateChecksum(l_vpdVector, 0, 7);

    // Product Info Area at offset 8 (112 bytes / 14 blocks of 8 bytes)
    size_t l_offset = 8;
    l_vpdVector[l_offset + 0] = 0x01; // Format Version
    l_vpdVector[l_offset + 1] = 0x0E; // Area Length in 8-byte units (112 bytes)
    l_vpdVector[l_offset + 2] = 0x19; // Language code: English

    // Type/Length + ASCII string helpers
    size_t l_pos = l_offset + 3;

    auto l_appendField = [&](const std::string& i_val) {
        l_vpdVector[l_pos++] = static_cast<uint8_t>(0xC0 | i_val.size());
        for (char l_ch : i_val)
        {
            l_vpdVector[l_pos++] = static_cast<uint8_t>(l_ch);
        }
    };

    l_appendField("Samsung");                    // MNAME
    l_appendField("PM9D3a");                     // PNAME
    l_appendField("SAMSUNG MZ3L61T9HFLT-00AW7"); // PPMN
    l_appendField("1.0");                        // PVER
    l_appendField("S6Z1NE0W123456");             // PSN
    l_appendField("ASSET123");                   // ASSET_TAG
    l_appendField("FRUFILE01");                  // FRU_FILE_ID

    l_vpdVector[l_pos++] = 0xC1;                 // End of fields sentinel

    // Area Checksum at the last byte of the 112-byte area (offset 8 + 112 - 1 =
    // 119)
    l_vpdVector[l_offset + 111] = calculateChecksum(l_vpdVector, l_offset, 111);

    return l_vpdVector;
}

} // namespace

TEST(IpmiFruParserTest, GoodTestCase)
{
    types::BinaryVector l_vpdVector = createSampleIpmiFruVpd();
    IpmiFruParser l_parser(l_vpdVector);

    auto l_parsedMap = l_parser.parse();
    auto l_ipmiVpdMapPtr = std::get_if<types::IPMIVpdMap>(&l_parsedMap);
    ASSERT_NE(l_ipmiVpdMapPtr, nullptr);

    const auto& l_fruMap = *l_ipmiVpdMapPtr;

    // 1. Validate Common Header parsing
    const auto& l_hdrOpt =
        l_fruMap[static_cast<size_t>(types::IpmiVpdAreaIndex::COMMON_HEADER)];
    ASSERT_TRUE(l_hdrOpt.has_value());
    auto l_hdrItr = l_hdrOpt->find("HEADER_FormatVersion");
    ASSERT_NE(l_hdrItr, l_hdrOpt->end());
    auto l_hdrVer = std::get_if<types::BinaryVector>(&l_hdrItr->second);
    ASSERT_NE(l_hdrVer, nullptr);
    EXPECT_EQ((*l_hdrVer)[0], 0x01);

    // 2. Validate Product Info Area parsing
    const auto& l_prodOpt = l_fruMap[static_cast<size_t>(
        types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)];
    ASSERT_TRUE(l_prodOpt.has_value());
    const auto& l_prodMap = *l_prodOpt;

    // Language Code
    auto l_langItr = l_prodMap.find("PLCODE");
    ASSERT_NE(l_langItr, l_prodMap.end());
    auto l_langCode = std::get_if<types::BinaryVector>(&l_langItr->second);
    ASSERT_NE(l_langCode, nullptr);
    EXPECT_EQ((*l_langCode)[0], 0x19);

    // Predefined 8-bit ASCII fields
    auto l_checkStringField =
        [&](const std::string& i_key, const std::string& i_expectedVal) {
            auto l_itr = l_prodMap.find(i_key);
            ASSERT_NE(l_itr, l_prodMap.end());
            auto l_val = std::get_if<std::string>(&l_itr->second);
            ASSERT_NE(l_val, nullptr);
            EXPECT_EQ(*l_val, i_expectedVal);
        };

    l_checkStringField("MNAME", "Samsung");
    l_checkStringField("PNAME", "PM9D3a");
    l_checkStringField("PPMN", "SAMSUNG MZ3L61T9HFLT-00AW7");
    l_checkStringField("PVER", "1.0");
    l_checkStringField("PSN", "S6Z1NE0W123456");
    l_checkStringField("ASSET_TAG", "ASSET123");
    l_checkStringField("FRU_FILE_ID", "FRUFILE01");
}

TEST(IpmiFruParserTest, ParseFromFile_MZ3L615THBLF)
{
    const std::string l_vpdFilePath = "vpd_files/MZ3L615THBLF-00AW7.bin";
    types::BinaryVector l_vpdVector = readVpdFile(l_vpdFilePath);

    IpmiFruParser l_parser(l_vpdVector);
    auto l_parsedMap = l_parser.parse();
    auto l_ipmiVpdMapPtr = std::get_if<types::IPMIVpdMap>(&l_parsedMap);
    ASSERT_NE(l_ipmiVpdMapPtr, nullptr);

    const auto& l_fruMap = *l_ipmiVpdMapPtr;

    // 1. Validate Common Header parsing
    const auto& l_hdrOpt =
        l_fruMap[static_cast<size_t>(types::IpmiVpdAreaIndex::COMMON_HEADER)];
    ASSERT_TRUE(l_hdrOpt.has_value());
    auto l_hdrItr = l_hdrOpt->find("HEADER_FormatVersion");
    ASSERT_NE(l_hdrItr, l_hdrOpt->end());
    auto l_hdrVer = std::get_if<types::BinaryVector>(&l_hdrItr->second);
    ASSERT_NE(l_hdrVer, nullptr);
    EXPECT_EQ((*l_hdrVer)[0], 0x01);

    // 2. Validate Product Info Area parsing
    const auto& l_prodOpt = l_fruMap[static_cast<size_t>(
        types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)];
    ASSERT_TRUE(l_prodOpt.has_value());
    const auto& l_prodMap = *l_prodOpt;

    // Language Code (0x19 = English)
    auto l_langItr = l_prodMap.find("PLCODE");
    ASSERT_NE(l_langItr, l_prodMap.end());
    auto l_langCode = std::get_if<types::BinaryVector>(&l_langItr->second);
    ASSERT_NE(l_langCode, nullptr);
    EXPECT_EQ((*l_langCode)[0], 0x19);

    // Predefined 8-bit ASCII fields
    auto l_checkStringField =
        [&](const std::string& i_key, const std::string& i_expectedVal) {
            auto l_itr = l_prodMap.find(i_key);
            ASSERT_NE(l_itr, l_prodMap.end());
            auto l_val = std::get_if<std::string>(&l_itr->second);
            ASSERT_NE(l_val, nullptr);
            EXPECT_EQ(*l_val, i_expectedVal);
        };

    l_checkStringField("MNAME", "Samsung");
    l_checkStringField("PNAME", "PM9D3a");
    l_checkStringField("PPMN", "SAMSUNG MZ3L615THBLF-00AW7");
    l_checkStringField("PSN", std::string(20, ' '));
}

TEST(IpmiFruParserTest, BufferTooSmall)
{
    // Buffer size less than COMMON_HEADER_SIZE (8 bytes)
    types::BinaryVector l_vpdVector = {0x01, 0x00, 0x00, 0x00};
    EXPECT_THROW(IpmiFruParser l_parser(l_vpdVector), DataException);
}

TEST(IpmiFruParserTest, HeaderVersionMismatch)
{
    // Header format version bits 3:0 not equal to 0x01
    types::BinaryVector l_vpdVector(8, 0x00);
    l_vpdVector[0] = 0x02; // Invalid format version
    l_vpdVector[7] = calculateChecksum(l_vpdVector, 0, 7);

    EXPECT_THROW(IpmiFruParser l_parser(l_vpdVector), DataException);
}

TEST(IpmiFruParserTest, HeaderInvalidChecksum)
{
    // Invalid Common Header Checksum
    types::BinaryVector l_vpdVector = createSampleIpmiFruVpd();
    l_vpdVector[7] =
        static_cast<uint8_t>(l_vpdVector[7] + 1); // Corrupt checksum

    IpmiFruParser l_parser(l_vpdVector);
    EXPECT_THROW(l_parser.parse(), DataException);
}

TEST(IpmiFruParserTest, ProductAreaVersionMismatch)
{
    // Corrupt Product Info Area format version byte
    types::BinaryVector l_vpdVector = createSampleIpmiFruVpd();
    l_vpdVector[8] = 0x02; // Invalid format version at offset 8
    l_vpdVector[119] = calculateChecksum(l_vpdVector, 8, 111);

    IpmiFruParser l_parser(l_vpdVector);
    auto l_parsedMap = l_parser.parse();
    auto l_ipmiVpdMapPtr = std::get_if<types::IPMIVpdMap>(&l_parsedMap);
    ASSERT_NE(l_ipmiVpdMapPtr, nullptr);

    // Product Info Area parsing should fail and remain unpopulated
    EXPECT_FALSE(
        (*l_ipmiVpdMapPtr)[static_cast<size_t>(
                               types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)]
            .has_value());
}

TEST(IpmiFruParserTest, ProductAreaInvalidChecksum)
{
    // Corrupt Product Info Area Checksum
    types::BinaryVector l_vpdVector = createSampleIpmiFruVpd();
    l_vpdVector[119] =
        static_cast<uint8_t>(l_vpdVector[119] + 1); // Corrupt checksum

    IpmiFruParser l_parser(l_vpdVector);
    auto l_parsedMap = l_parser.parse();
    auto l_ipmiVpdMapPtr = std::get_if<types::IPMIVpdMap>(&l_parsedMap);
    ASSERT_NE(l_ipmiVpdMapPtr, nullptr);

    // Product Info Area parsing should fail due to invalid checksum
    EXPECT_FALSE(
        (*l_ipmiVpdMapPtr)[static_cast<size_t>(
                               types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)]
            .has_value());
}

TEST(IpmiFruParserTest, ProductAreaTruncated)
{
    // Declare area size that extends beyond the provided buffer
    types::BinaryVector l_vpdVector = createSampleIpmiFruVpd();
    l_vpdVector.resize(
        50); // Truncate vector below declared area size (120 bytes)
    l_vpdVector[7] = calculateChecksum(l_vpdVector, 0, 7);

    IpmiFruParser l_parser(l_vpdVector);
    auto l_parsedMap = l_parser.parse();
    auto l_ipmiVpdMapPtr = std::get_if<types::IPMIVpdMap>(&l_parsedMap);
    ASSERT_NE(l_ipmiVpdMapPtr, nullptr);

    EXPECT_FALSE(
        (*l_ipmiVpdMapPtr)[static_cast<size_t>(
                               types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)]
            .has_value());
}

TEST(IpmiFruParserTest, ProductAreaEarlyEndOfFields)
{
    // Check that End of Record sentinel (0xC1) stops further field parsing
    // gracefully
    types::BinaryVector l_vpdVector(120, 0x00);
    l_vpdVector[0] = 0x01;
    l_vpdVector[4] = 0x01;
    l_vpdVector[7] = calculateChecksum(l_vpdVector, 0, 7);

    size_t l_offset = 8;
    l_vpdVector[l_offset + 0] = 0x01;
    l_vpdVector[l_offset + 1] = 0x0E;
    l_vpdVector[l_offset + 2] = 0x19;

    size_t l_pos = l_offset + 3;
    // Only provide MNAME then End of Record (0xC1)
    std::string l_mname = "Samsung";
    l_vpdVector[l_pos++] = static_cast<uint8_t>(0xC0 | l_mname.size());
    for (char l_ch : l_mname)
    {
        l_vpdVector[l_pos++] = static_cast<uint8_t>(l_ch);
    }
    l_vpdVector[l_pos++] = 0xC1; // Early sentinel

    l_vpdVector[l_offset + 111] = calculateChecksum(l_vpdVector, l_offset, 111);

    IpmiFruParser l_parser(l_vpdVector);
    auto l_parsedMap = l_parser.parse();
    auto l_ipmiVpdMapPtr = std::get_if<types::IPMIVpdMap>(&l_parsedMap);
    ASSERT_NE(l_ipmiVpdMapPtr, nullptr);

    const auto& l_prodOpt = (*l_ipmiVpdMapPtr)[static_cast<size_t>(
        types::IpmiVpdAreaIndex::PRODUCT_INFO_AREA)];
    ASSERT_TRUE(l_prodOpt.has_value());
    EXPECT_NE(l_prodOpt->find("MNAME"), l_prodOpt->end());
    EXPECT_EQ(l_prodOpt->find("PNAME"), l_prodOpt->end());
}
