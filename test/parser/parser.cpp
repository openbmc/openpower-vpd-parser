#include <cassert>
#include <defines.hpp>
#include <fstream>
#include <iterator>
#include <store.hpp>
#include <vpd-parser/ipz_parser.hpp>

void runTests()
{
    using namespace openpower::vpd;
    using namespace openpower::vpd::ipz::parser;
    // Test parse() API
    {
        std::ifstream vpdFile("test.vpd", std::ios::binary);
        Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
                   std::istreambuf_iterator<char>());

        std::string path = "/dummyPath";
        IpzVpdParser ipzParser(std::move(vpd), path);
        auto vpdStore = std::move(std::get<Store>(ipzParser.parse()));

        assert(("P012" == vpdStore.get<Record::VINI, record::Keyword::CC>()));
        assert(("2019-01-01-08:30:00" ==
                vpdStore.get<Record::VINI, record::Keyword::MB>()));
    }
}

int main()
{
    runTests();

    return 0;
}
