#include <cassert>
#include <defines.hpp>
#include <fstream>
#include <impl.hpp>
#include <iterator>
#include <parser.hpp>
#include <store.hpp>

#include <gtest/gtest.h>

using namespace openpower::vpd;

TEST(IpzVpdParserApp, vpdGoodPath)
{
    // TODO 1: Hardcoded local file, create a binary file ?
    std::ifstream vpdFile("./4900.dat", std::ios::binary);
    Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
               std::istreambuf_iterator<char>());

    // call app for this vpd
    parser::Impl p(std::move(vpd));
    Store vpdStore = p.run();

    static const std::string record = "VINI";
    static const std::string keyword = "DR";

    // TODO 2: Move this as an utility to store.hpp
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
