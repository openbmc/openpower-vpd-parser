#include "vpdecc.h"

#include "ddimm_parser.hpp"
#include "exceptions.hpp"
#include "parser.hpp"
#include "types.hpp"

#include <cstdint>
#include <exception>
#include <fstream>

#include <gtest/gtest.h>

using namespace vpd;

TEST(DdimmVpdParserTest, GoodTestCase)
{
    types::DdimmVpdMap l_ddimmMap{
        std::pair<std::string, size_t>{"MemorySizeInKB", 0x2000000},
        std::pair<std::string, types::BinaryVector>{
            "FN", {0x30, 0x33, 0x48, 0x44, 0x37, 0x30, 0x30}},
        std::pair<std::string, types::BinaryVector>{
            "PN", {0x30, 0x33, 0x48, 0x44, 0x37, 0x30, 0x30}},
        std::pair<std::string, types::BinaryVector>{"SN",
                                                    {0x59, 0x48, 0x33, 0x33,
                                                     0x31, 0x54, 0x33, 0x38,
                                                     0x34, 0x30, 0x33, 0x46}},
        std::pair<std::string, types::BinaryVector>{"CC",
                                                    {0x33, 0x32, 0x41, 0x31}}};

    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ddr5_ddimm.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    ASSERT_EQ(1,
              l_ddimmMap == std::get<types::DdimmVpdMap>(l_vpdParser.parse()));
}

TEST(DdimmVpdParserTest, DDR4GoodTestCase)
{
    types::DdimmVpdMap l_ddimmMap{
        std::pair<std::string, size_t>{"MemorySizeInKB", 0x4000000},
        std::pair<std::string, types::BinaryVector>{
            "FN", {0x37, 0x38, 0x50, 0x36, 0x35, 0x37, 0x35}},
        std::pair<std::string, types::BinaryVector>{
            "PN", {0x37, 0x38, 0x50, 0x36, 0x35, 0x37, 0x35}},
        std::pair<std::string, types::BinaryVector>{"SN",
                                                    {0x59, 0x48, 0x33, 0x35,
                                                     0x31, 0x54, 0x31, 0x35,
                                                     0x53, 0x30, 0x44, 0x35}},
        std::pair<std::string, types::BinaryVector>{"CC",
                                                    {0x33, 0x32, 0x37, 0x42}}};

    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ddr4_ddimm.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    ASSERT_EQ(1,
              l_ddimmMap == std::get<types::DdimmVpdMap>(l_vpdParser.parse()));
}

TEST(DdimmVpdParserTest, InvalidDdrType)
{
    // Invalid DDR type, corrupted at index[2]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ddr5_ddimm_corrupted_index_2.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(DdimmVpdParserTest, ZeroDdimmSize)
{
    // Badly formed DDIMM VPD data - corrupted at index[235],
    // ddimm size calculated a zero
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ddr5_ddimm_corrupted_index_235.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(DdimmVpdParserTest, InvalidDensityPerDie)
{
    // Out of range data, fails to check valid value - corrupted at index[4]
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ddr5_ddimm_corrupted_index_4.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(DdimmVpdParserTest, InvalidVpdType)
{
    // Invalid VPD type - corrupted at index[2] & index[3]
    // Not able to find the VPD type, vpdTypeCheck failed
    nlohmann::json l_json;
    std::string l_vpdFile("vpd_files/ddr5_ddimm_corrupted_index_2_3.dat");
    Parser l_vpdParser(l_vpdFile, l_json);

    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(DdimmVpdParserTest, EmptyInputVector)
{
    // Blank VPD
    types::BinaryVector emptyVector{};

    EXPECT_THROW(DdimmVpdParser(std::move(emptyVector)), DataException);
}

int main(int i_argc, char** io_argv)
{
    ::testing::InitGoogleTest(&i_argc, io_argv);

    return RUN_ALL_TESTS();
}
