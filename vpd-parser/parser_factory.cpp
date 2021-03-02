#include "parser_factory.hpp"

#include "ipz_parser.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "utils.hpp"
#include "vpd_exceptions.hpp"

#include <fstream>

using namespace vpd::keyword::parser;
using namespace openpower::vpd::memory::parser;
using namespace openpower::vpd::parser::interface;
using namespace openpower::vpd::ipz::parser;
using namespace openpower::vpd::exceptions;

namespace openpower
{
namespace vpd
{
namespace parser
{
namespace factory
{
std::string ParserFactory::badVpdPath{};
interface::ParserInterface* ParserFactory::getParser(Binary&& vpdVector,
                                                     string badVpd)
{
    ParserFactory::badVpdPath = badVpd;
    vpdType type = vpdTypeCheck(vpdVector);
    ofstream badVpdFileStream(badVpdPath, ofstream::binary);
    badVpdFileStream.write(reinterpret_cast<char*>(&vpdVector[0]),
                           vpdVector.size());

    switch (type)
    {
        case IPZ_VPD:
        {
            return new IpzVpdParser(std::move(vpdVector));
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
            throw VpdDataException("Unable to determine VPD format");
    }
}

void ParserFactory::freeParser(interface::ParserInterface* parser)
{
    if (parser)
    {
        std::remove(ParserFactory::badVpdPath.c_str());
        delete parser;
        parser = nullptr;
    }
}

} // namespace factory
} // namespace parser
} // namespace vpd
} // namespace openpower
