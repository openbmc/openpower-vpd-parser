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
    Impl p(std::move(vpd), vpdFilePath);
    Store s = p.run();
    return s;
}

void IpzVpdParser::processHeader()
{
    Impl p(std::move(vpd), vpdFilePath);
    p.checkVPDHeader();
}

std::string IpzVpdParser::getInterfaceName() const
{
    return ipzVpdInf;
}

} // namespace parser
} // namespace ipz
} // namespace vpd
} // namespace openpower
