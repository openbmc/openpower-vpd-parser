#include "parser_factory.hpp"

#include "const.hpp"
#include "ibm_vpd_utils.hpp"
#include "ipz_parser.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "vpd_exceptions.hpp"

using namespace vpd;
using namespace openpower::vpd::exceptions;
using namespace openpower::vpd::constants;

namespace openpower
{
namespace vpd
{
ParserInterface* ParserFactory::getParser(const types::Binary& vpdVector,
                                          const std::string& inventoryPath)
{
    vpdType type = vpdTypeCheck(vpdVector);

    switch (type)
    {
        case IPZ_VPD:
        {
            return new IpzVpdParser(vpdVector, inventoryPath);
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

void ParserFactory::freeParser(ParserInterface* parser)
{
    if (parser)
    {
        delete parser;
        parser = nullptr;
    }
}

} // namespace vpd
} // namespace openpower
