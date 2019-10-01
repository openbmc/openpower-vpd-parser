#include <cassert>
#include <defines.hpp>
#include <fstream>
#include <impl.hpp>
#include <iterator>
#include <parser.hpp>
#include <store.hpp>

#include <gtest/gtest.h>
#include <filesystem>

using namespace openpower::vpd;
using namespace std;
namespace fs = std::filesystem;

TEST(IpzVpdParserApp, vpdGoodPath)
{
    // TODO 1: Hardcoded local file, create a binary file ?
    std::cout << "Current path is " << fs::current_path() << '\n';
  
    for(auto& p: fs::directory_iterator(fs::current_path()))
        std::cout << p.path() << '\n';
    
    std::ifstream vpdFile("./4900.dat",
                          std::ifstream::in | std::ifstream::binary);
    if (vpdFile.is_open())
    {
        cout << "Failed to open \n";
    }
    else
    {
        cout << "Opened file \n";
    }
    Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
               std::istreambuf_iterator<char>());

    cout << "size:" << vpd.size() << "\n";
    cout << "Reached before p \n";
    // call app for this vpd
    parser::Impl p(std::move(vpd));
    cout << "Reached before run \n";
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
