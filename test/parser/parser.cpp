#include <cassert>
#include <defines.hpp>
#include <fstream>
#include <iterator>
#include <parser.hpp>
#include <store.hpp>

void runTests()
{
    using namespace openpower::vpd;

    // Test parse() API
    {
        std::ifstream vpdFile("test.vpd", std::ios::binary);
        Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
                   std::istreambuf_iterator<char>());

        auto vpdStore = parse(std::move(vpd));

        assert(("P012" == vpdStore.get<Record::VINI, record::Keyword::CC>()));
    }
}

int main()
{
    runTests();

    return 0;
}
