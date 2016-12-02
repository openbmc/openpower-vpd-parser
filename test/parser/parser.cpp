#include <defines.hpp>
#include <store.hpp>
#include <parser.hpp>
#include <cassert>
#include <fstream>
#include <iterator>

void runTests()
{
    using namespace openpower::vpd;

    // Test parse() API
    {
        std::ifstream vpdFile("test.vpd", std::ios::binary);
        Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
                   std::istreambuf_iterator<char>());

        auto vpdStore = parse(std::move(vpd));

        assert(("P012" ==
               vpdStore.get<Record::VINI, record::Keyword::CC>()));
    }
}

int main()
{
    runTests();

    return 0;
}
