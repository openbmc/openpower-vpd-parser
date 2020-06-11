#include "impl.hpp"
#include "opnepower_parser.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{
using namespace openpower::vpd::parser;
using namespace openpower::vpd::constants;

std::variant<kwdVpdMap, Store> OpenpowerVpdParser::parse()
{
    Impl p(std::move(vpd));
    Store s = p.run();
    return s;
}

std::string OpenpowerVpdParser::getInterfaceName() const
{
    return ipzVpdInf;
}

} // namespace parser
} // namespace vpd
} // namespace openpower
