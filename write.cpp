#include <defines.hpp>
#include <writefru.hpp>
#include <exception>
#include <algorithm>
#include <write.hpp>

namespace openpower
{
namespace vpd
{
namespace inventory
{

static const std::unordered_map<std::string, Fru> supportedFrus =
{
    {"BMC", Fru::BMC},
    {"ETHERNET", Fru::ETHERNET}
};

void write(const std::string& type,
           const Store& vpdStore,
           const std::string& path)
{
    // Get the enum corresponding to type, and call
    // appropriate write FRU method.

    auto fru = type;
    std::transform(fru.begin(), fru.end(), fru.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    if(supportedFrus.end() == supportedFrus.find(fru))
    {
        throw std::runtime_error("Unsupported FRU: " + std::move(fru));
    }
    else
    {
        switch (supportedFrus.at(fru))
        {
            case Fru::BMC:
            {
                writeFru<Fru::BMC>(std::move(vpdStore), path);
                break;
            }

            case Fru::ETHERNET:
            {
                writeFru<Fru::ETHERNET>(std::move(vpdStore), path);
                break;
            }

            default:
                break;
        }
    }
}

} // inventory
} // namespace vpd
} // namespace openpower
