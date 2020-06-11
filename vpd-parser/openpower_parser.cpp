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

void OpenpowerVpdParser::write(const std::string& type, const Store& vpdStore,
                               const std::string& path)
{
    // Get the enum corresponding to type, and call
    // appropriate write FRU method.

    std::string fru = type;
    std::transform(fru.begin(), fru.end(), fru.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    auto iterator = SupportedFrus.find(fru);
    if (SupportedFrus.end() == iterator)
    {
        throw std::runtime_error("Unsupported FRU: " + std::move(fru));
    }
    else
    {
        switch (iterator->second)
        {
            case Fru::BMC:
            {
                // writeFru<Fru::BMC>(vpdStore, path);
                break;
            }

            case Fru::ETHERNET:
            {
                //  writeFru<Fru::ETHERNET>(vpdStore, path);
                break;
            }

            case Fru::ETHERNET1:
            {
                // writeFru<Fru::ETHERNET1>(vpdStore, path);
                break;
            }

            default:
                break;
        }
    }
}

} // namespace parser
} // namespace vpd
} // namespace openpower
