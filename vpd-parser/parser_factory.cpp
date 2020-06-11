#include "parser_factory.hpp"

#include "ipz_parser.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "opnepower_parser.hpp"
#include "utils.hpp"

using namespace vpd::keyword::parser;
using namespace openpower::vpd::memory::parser;
using namespace openpower::vpd::parser::interface;
using namespace openpower::vpd::ipz::parser;
using namespace openpower::vpd::parser;

namespace openpower
{
namespace vpd
{
namespace parser
{
namespace factory
{
interface::ParserInterface* ParserFactory::getParser(Binary&& vpdVector)
{
    vpdType type = vpdTypeCheck(vpdVector);

    switch (type)
    {
        case IPZ_VPD:
        {
#ifdef DIPZ_PARSER
            return new IpzVpdParser(std::move(vpdVector));
#else
            return new OpenpowerVpdParser(std::move(vpdVector));
#endif
        }

        case KEYWORD_VPD:
        {
            return new KeywordVpdParser(std::move(vpdVector));
        }

        case MEMORY_VPD:
        {
            return new memoryVpdParser(std::move(vpdVector));
        }

        default:
            throw std::runtime_error("Invalid VPD format");
    }
}

void ParserFactory::freeParser(interface::ParserInterface* parser)
{
    if (parser)
    {
        delete parser;
    }
}

} // namespace factory
} // namespace parser
} // namespace vpd
} // namespace openpower
