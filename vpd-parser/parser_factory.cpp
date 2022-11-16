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

namespace openpower
{
namespace vpd
{
namespace parser
{
namespace factory
{
interface::ParserInterface* ParserFactory::getParser(
    const Binary& vpdVector, const std::string& inventoryPath,
    const std::string& vpdFilePath, uint32_t vpdStartOffset)
{
    vpdType type = vpdTypeCheck(vpdVector);

    switch (type)
    {
        case IPZ_VPD:
        {
            return new IpzVpdParser(vpdVector, inventoryPath, vpdFilePath,
                                    vpdStartOffset);
        }

        case KEYWORD_VPD:
        {
            return new KeywordVpdParser(vpdVector);
        }

        case MEMORY_VPD:
        {
            return new memoryVpdParser(vpdVector);
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
