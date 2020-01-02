#include <defines.hpp>
#include <fstream>
#include <impl.hpp>
#include <parser.hpp>
#include <store.hpp>

#include <gtest/gtest.h>

using namespace openpower::vpd;

TEST(IpzVpdParserApp, vpdGoodPath)
{
    // Get the vpd
    std::ifstream vpdFile("./ipz.vpd", std::ios::binary);
 
    Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
               std::istreambuf_iterator<char>());

    // call app for this vpd
    parser::Impl p(std::move(vpd));
    Store vpdStore = p.run();

    static const std::string record = "VINI";
    static const std::string keyword = "DR";

    std::string dataFound;
    Parsed st_bin = vpdStore.get();

    auto kw = st_bin.find(record);
    if (st_bin.end() != kw)
    {
        auto value = (kw->second).find(keyword);
        if ((kw->second).end() != value)
        {
            dataFound = value->second;
        }
    }

    ASSERT_EQ(dataFound, "APSS & TPM  CARD");
}

TEST(IpzVpdParserApp, vpdBadPathEmptyVPD)
{
    Binary vpd = {};

    // VPD is empty
    parser::Impl p(std::move(vpd));

    // Expecting a throw here
    EXPECT_THROW(p.run(), std::runtime_error);
}

TEST(IpzVpdParserApp, vpdBadPathMissingHeader)
{
    // Get the vpd
    std::ifstream vpdFile("./ipz.vpd", std::ios::binary);
 
    Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
               std::istreambuf_iterator<char>());

    // corrupt the VHDR
    vpd[17] = 0x00;

    parser::Impl p(std::move(vpd));

    // Expecting a throw here
    EXPECT_THROW(p.run(), std::runtime_error);
}

TEST(IpzVpdParserApp, vpdBadPathMissingVTOC)
{
    // Get the vpd
    std::ifstream vpdFile("./ipz.vpd", std::ios::binary);
 
    Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
               std::istreambuf_iterator<char>());

    // corrupt the VTOC
    vpd[61] = 0x00;

    parser::Impl p(std::move(vpd));

    // Expecting a throw here
    EXPECT_THROW(p.run(), std::runtime_error);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
