#include "ipz_parser.hpp"

#include "impl.hpp"

namespace openpower
{
namespace vpd
{

using namespace openpower::vpd::parser;
using namespace openpower::vpd::constants;

std::variant<types::KeywordVpdMap, Store> IpzVpdParser::parse()
{
    Impl p(vpd, inventoryPath);
    Store s = p.run();
    return s;
}

void IpzVpdParser::processHeader()
{
    Impl p(vpd, inventoryPath);
    p.checkVPDHeader();
}

std::string IpzVpdParser::getInterfaceName() const
{
    return ipzVpdInf;
}

} // namespace vpd
} // namespace openpower
