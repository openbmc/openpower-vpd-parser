#include "parser_factory.hpp"

#include "const.hpp"
#include "ibm_vpd_utils.hpp"
#include "ipz_parser.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "vpd_exceptions.hpp"

using namespace vpd::keyword::parser;
using namespace openpower::vpd::memory::parser;
using namespace openpower::vpd::parser::interface;
using namespace openpower::vpd::ipz::parser;
using namespace openpower::vpd::exceptions;
using namespace openpower::vpd::constants;
using namespace openpower::vpd::inventory;

namespace openpower
{
namespace vpd
{
namespace parser
{
namespace factory
{
interface::ParserInterface* ParserFactory::getParser(Binary&& vpdVector,
                                                     Path& filePath)
{
    vpdType type = vpdTypeCheck(vpdVector);

    switch (type)
    {
        case IPZ_VPD:
        {
            return new IpzVpdParser(std::move(vpdVector), filePath);
        }

        case KEYWORD_VPD:
        {
            return new KeywordVpdParser(std::move(vpdVector), filePath);
        }

        case MEMORY_VPD:
        {
            return new memoryVpdParser(std::move(vpdVector), filePath);
        }

        default:
            throw VpdDataException("Unable to determine VPD format");
    }
}

void ParserFactory::freeParser(interface::ParserInterface* parser)
{
    if (parser)
    {
        delete parser;
        parser = nullptr;
    }
}

} // namespace factory
} // namespace parser
} // namespace vpd
} // namespace openpower
