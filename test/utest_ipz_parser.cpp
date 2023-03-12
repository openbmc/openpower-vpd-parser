#include "ipz_parser.hpp"
#include "parser.hpp"

#include <exception>

#include <gtest/gtest.h>

TEST(IpzVpdParserTest, GoodTestCase)
{
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ipz_system.dat");
    vpd::Parser l_vpdParser(l_vpdFile, l_json);

    vpd::types::IPZVpdMap l_ipzVpdMap;
    auto l_parsedMap = l_vpdParser.parse();
    if (auto l_ipzVpdMapPtr = std::get_if<vpd::types::IPZVpdMap>(&l_parsedMap))
        l_ipzVpdMap = *l_ipzVpdMapPtr;

    std::string l_record("VINI");
    std::string l_keyword("DR");
    std::string l_description;

    // check 'DR' keyword value from 'VINI' record
    auto l_vpdItr = l_ipzVpdMap.find(l_record);
    if (l_ipzVpdMap.end() != l_vpdItr)
    {
        auto l_kwValItr = (l_vpdItr->second).find(l_keyword);
        if ((l_vpdItr->second).end() != l_kwValItr)
        {
            l_description = l_kwValItr->second;
        }
    }
    EXPECT_EQ(l_description, "SYSTEM BACKPLANE");

    // check 'SN' keyword value from 'VINI' record
    l_record = "VINI";
    l_keyword = "SN";
    l_vpdItr = l_ipzVpdMap.find(l_record);
    if (l_ipzVpdMap.end() != l_vpdItr)
    {
        auto l_kwValItr = (l_vpdItr->second).find(l_keyword);
        if ((l_vpdItr->second).end() != l_kwValItr)
        {
            l_description = l_kwValItr->second;
        }
    }
    EXPECT_EQ(l_description, "Y131UF07300L");

    // check 'DR' keyword value of 'VSYS' record
    l_record = "VSYS";
    l_keyword = "DR";
    l_vpdItr = l_ipzVpdMap.find(l_record);
    if (l_ipzVpdMap.end() != l_vpdItr)
    {
        auto l_kwValItr = (l_vpdItr->second).find(l_keyword);
        if ((l_vpdItr->second).end() != l_kwValItr)
        {
            l_description = l_kwValItr->second;
        }
    }
    ASSERT_EQ(l_description, "SYSTEM");
}

TEST(IpzVpdParserTest, VpdFileDoesNotExist)
{
    // Vpd file does not exist
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/xyz.dat");

    EXPECT_THROW(vpd::Parser(l_vpdFile, l_json), std::runtime_error);
}

TEST(IpzVpdParserTest, MissingHeader)
{
    // Missing VHDR tag, failed header check - corrupted at index[17]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ipz_system_corrupted_index_17.dat");
    vpd::Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(IpzVpdParserTest, MissingVtoc)
{
    // Missing VTOC tag - corrupted at index[61]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ipz_system_corrupted_index_61.dat");
    vpd::Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(IpzVpdParserTest, MalformedVpdFile)
{
    // Vpd vector size is less than RECORD_MIN(44), fails for checkHeader
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ipz_system_min_record.dat");
    vpd::Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

#ifdef IPZ_ECC_CHECK
TEST(IpzVpdParserTest, InvalidRecordOffset)
{
    // VTOC ECC check fail
    // Invalid VINI Record offset, corrupted at index[74]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ipz_system_corrupted_index_74.dat");
    vpd::Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(IpzVpdParserTest, InvalidRecordEccOffset)
{
    // VTOC ECC check fail
    // Invalid VINI Record ECC offset, corrupted at index[78] & index[79]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ipz_system_corrupted_index_78_79.dat");
    vpd::Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(IpzVpdParserTest, TruncatedVpdFile)
{
    // Truncated vpd file, VTOC ECC check fail
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ipz_system_truncated.dat");
    vpd::Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}
#endif
