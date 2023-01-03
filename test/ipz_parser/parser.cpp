#include "ipz_parser.hpp"

#include <cassert>
#include <defines.hpp>
#include <fstream>
#include <impl.hpp>
#include <iterator>
#include <store.hpp>

#include <gtest/gtest.h>

using namespace openpower::vpd;
using namespace openpower::vpd::types;

TEST(IpzVpdParserApp, vpdGoodPath)
{
    // Create a vpd
    Binary vpd = {
        0x00, 0x0f, 0x17, 0xba, 0x42, 0xca, 0x82, 0xd7, 0x7b, 0x77, 0x1e, 0x84,
        0x28, 0x00, 0x52, 0x54, 0x04, 0x56, 0x48, 0x44, 0x52, 0x56, 0x44, 0x02,
        0x30, 0x31, 0x50, 0x54, 0x0e, 0x56, 0x54, 0x4f, 0x43, 0xd5, 0x00, 0x37,
        0x00, 0x4c, 0x00, 0x97, 0x05, 0x13, 0x00, 0x50, 0x46, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x84, 0x48, 0x00, 0x52, 0x54,
        0x04, 0x56, 0x54, 0x4f, 0x43, 0x50, 0x54, 0x0e, 0x56, 0x49, 0x4e, 0x49,
        0xd5, 0x00, 0x52, 0x00, 0x90, 0x00, 0x73, 0x05, 0x24, 0x00, 0x84, 0x8c,
        0x00, 0x52, 0x54, 0x04, 0x56, 0x49, 0x4e, 0x49, 0x44, 0x52, 0x10, 0x41,
        0x50, 0x53, 0x53, 0x20, 0x26, 0x20, 0x54, 0x50, 0x4d, 0x20, 0x20, 0x43,
        0x41, 0x52, 0x44, 0x43, 0x45, 0x01, 0x31, 0x56, 0x5a, 0x02, 0x30, 0x31,
        0x46, 0x4e, 0x07, 0x30, 0x31, 0x44, 0x48, 0x32, 0x30, 0x30, 0x50, 0x4e,
        0x07, 0x30, 0x31, 0x44, 0x48, 0x32, 0x30, 0x31, 0x53, 0x4e, 0x0c, 0x59,
        0x4c, 0x33, 0x30, 0x42, 0x47, 0x37, 0x43, 0x46, 0x30, 0x33, 0x50, 0x43,
        0x43, 0x04, 0x36, 0x42, 0x36, 0x36, 0x50, 0x52, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x45, 0x04, 0x30, 0x30, 0x30, 0x31,
        0x43, 0x54, 0x04, 0x40, 0xb8, 0x02, 0x03, 0x48, 0x57, 0x02, 0x00, 0x01,
        0x42, 0x33, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x34, 0x01,
        0x00, 0x42, 0x37, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x46, 0x02, 0x00, 0x00, 0x78, 0x84, 0xdc,
        0x00, 0x52, 0x54, 0x04};

    // call app for this vpd
    parser::Impl p(std::move(vpd), std::string{});
    Store vpdStore = p.run();

    static const std::string record = "VINI";
    static const std::string keyword = "DR";

    // TODO 2: Move this as an utility to store.hpp
    std::string dataFound;
    Parsed st_bin = vpdStore.getVpdMap();

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
    parser::Impl p(std::move(vpd), std::string{});

    // Expecting a throw here
    EXPECT_THROW(p.run(), std::runtime_error);
}

TEST(IpzVpdParserApp, vpdBadPathMissingHeader)
{
    Binary vpd = {
        0x00, 0x0f, 0x17, 0xba, 0x42, 0xca, 0x82, 0xd7, 0x7b, 0x77, 0x1e, 0x84,
        0x28, 0x00, 0x52, 0x54, 0x04, 0x56, 0x48, 0x44, 0x52, 0x56, 0x44, 0x02,
        0x30, 0x31, 0x50, 0x54, 0x0e, 0x56, 0x54, 0x4f, 0x43, 0xd5, 0x00, 0x37,
        0x00, 0x4c, 0x00, 0x97, 0x05, 0x13, 0x00, 0x50, 0x46, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x84, 0x48, 0x00, 0x52, 0x54,
        0x04, 0x56, 0x54, 0x4f, 0x43, 0x50, 0x54, 0x0e, 0x56, 0x49, 0x4e, 0x49,
        0xd5, 0x00, 0x52, 0x00, 0x90, 0x00, 0x73, 0x05, 0x24, 0x00, 0x84, 0x8c,
        0x00, 0x52, 0x54, 0x04, 0x56, 0x49, 0x4e, 0x49, 0x44, 0x52, 0x10, 0x41,
        0x50, 0x53, 0x53, 0x20, 0x26, 0x20, 0x54, 0x50, 0x4d, 0x20, 0x20, 0x43,
        0x41, 0x52, 0x44, 0x43, 0x45, 0x01, 0x31, 0x56, 0x5a, 0x02, 0x30, 0x31,
        0x46, 0x4e, 0x07, 0x30, 0x31, 0x44, 0x48, 0x32, 0x30, 0x30, 0x50, 0x4e,
        0x07, 0x30, 0x31, 0x44, 0x48, 0x32, 0x30, 0x31, 0x53, 0x4e, 0x0c, 0x59,
        0x4c, 0x33, 0x30, 0x42, 0x47, 0x37, 0x43, 0x46, 0x30, 0x33, 0x50, 0x43,
        0x43, 0x04, 0x36, 0x42, 0x36, 0x36, 0x50, 0x52, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x45, 0x04, 0x30, 0x30, 0x30, 0x31,
        0x43, 0x54, 0x04, 0x40, 0xb8, 0x02, 0x03, 0x48, 0x57, 0x02, 0x00, 0x01,
        0x42, 0x33, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x34, 0x01,
        0x00, 0x42, 0x37, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x46, 0x02, 0x00, 0x00, 0x78, 0x84, 0xdc,
        0x00, 0x52, 0x54, 0x04};

    // corrupt the VHDR
    vpd[17] = 0x00;

    parser::Impl p(std::move(vpd), std::string{});

    // Expecting a throw here
    EXPECT_THROW(p.run(), std::runtime_error);
}

TEST(IpzVpdParserApp, vpdBadPathMissingVTOC)
{
    Binary vpd = {
        0x00, 0x0f, 0x17, 0xba, 0x42, 0xca, 0x82, 0xd7, 0x7b, 0x77, 0x1e, 0x84,
        0x28, 0x00, 0x52, 0x54, 0x04, 0x56, 0x48, 0x44, 0x52, 0x56, 0x44, 0x02,
        0x30, 0x31, 0x50, 0x54, 0x0e, 0x56, 0x54, 0x4f, 0x43, 0xd5, 0x00, 0x37,
        0x00, 0x4c, 0x00, 0x97, 0x05, 0x13, 0x00, 0x50, 0x46, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x84, 0x48, 0x00, 0x52, 0x54,
        0x04, 0x56, 0x54, 0x4f, 0x43, 0x50, 0x54, 0x0e, 0x56, 0x49, 0x4e, 0x49,
        0xd5, 0x00, 0x52, 0x00, 0x90, 0x00, 0x73, 0x05, 0x24, 0x00, 0x84, 0x8c,
        0x00, 0x52, 0x54, 0x04, 0x56, 0x49, 0x4e, 0x49, 0x44, 0x52, 0x10, 0x41,
        0x50, 0x53, 0x53, 0x20, 0x26, 0x20, 0x54, 0x50, 0x4d, 0x20, 0x20, 0x43,
        0x41, 0x52, 0x44, 0x43, 0x45, 0x01, 0x31, 0x56, 0x5a, 0x02, 0x30, 0x31,
        0x46, 0x4e, 0x07, 0x30, 0x31, 0x44, 0x48, 0x32, 0x30, 0x30, 0x50, 0x4e,
        0x07, 0x30, 0x31, 0x44, 0x48, 0x32, 0x30, 0x31, 0x53, 0x4e, 0x0c, 0x59,
        0x4c, 0x33, 0x30, 0x42, 0x47, 0x37, 0x43, 0x46, 0x30, 0x33, 0x50, 0x43,
        0x43, 0x04, 0x36, 0x42, 0x36, 0x36, 0x50, 0x52, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x45, 0x04, 0x30, 0x30, 0x30, 0x31,
        0x43, 0x54, 0x04, 0x40, 0xb8, 0x02, 0x03, 0x48, 0x57, 0x02, 0x00, 0x01,
        0x42, 0x33, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x34, 0x01,
        0x00, 0x42, 0x37, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x50, 0x46, 0x02, 0x00, 0x00, 0x78, 0x84, 0xdc,
        0x00, 0x52, 0x54, 0x04};

    // corrupt the VTOC
    vpd[61] = 0x00;

    parser::Impl p(std::move(vpd), std::string{});

    // Expecting a throw here
    EXPECT_THROW(p.run(), std::runtime_error);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
