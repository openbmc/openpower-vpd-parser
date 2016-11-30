#include <cassert>
#include <string>
#include <unordered_map>
#include <utility>
#include "defines.hpp"
#include "store.hpp"

void runTests()
{
    using namespace openpower::vpd;

    // Test Store::get API
    {
        Parsed vpd;
        using inner = Parsed::mapped_type;
        inner i;

        i.emplace("SN", "1001");
        i.emplace("PN", "F001");
        i.emplace("DR", "Fake FRU");
        vpd.emplace("VINI", i);

        Store s(std::move(vpd));

        assert(("1001" == s.get<Record::VINI, record::Keyword::SN>()));
        assert(("F001" == s.get<Record::VINI, record::Keyword::PN>()));
        assert(("Fake FRU" == s.get<Record::VINI, record::Keyword::DR>()));
    }
}

int main()
{
    runTests();

    return 0;
}
