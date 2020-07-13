#include "ipz_parser.hpp"

#include "impl.hpp"

namespace openpower
{
namespace vpd
{
namespace ipz
{
namespace parser
{
using namespace openpower::vpd::parser;
using namespace openpower::vpd::constants;

std::variant<kwdVpdMap, Store> IpzVpdParser::parse()
{
    Impl p(std::move(vpd));
    Store s = p.run();
    return s;
}

void IpzVpdParser::processHeader()
{
    Impl p(std::move(vpd));
    p.checkVPDHeader();
}

std::string IpzVpdParser::getInterfaceName() const
{
    return ipzVpdInf;
}

Binary IpzVpdParser::fixEcc()
{
    Impl ecc(std::move(vpd));
    std::cout << "\n calling impl cpp " << std::endl;
    Binary temp = ecc.fixEccImpl();
    return Binary(std::move(temp));
}
} // namespace parser
} // namespace ipz
} // namespace vpd
} // namespace openpower
