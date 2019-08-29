#include "write.hpp"

#include "defines.hpp"
#include "writefru.hpp"

#include <algorithm>
#include <exception>

namespace openpower
{
namespace vpd
{
namespace inventory
{

// Some systems have two MAC addresses
static const std::unordered_map<std::string, Fru> supportedFrus = {
    {"BMC", Fru::BMC},
    {"ETHERNET", Fru::ETHERNET},
    {"ETHERNET1", Fru::ETHERNET1}};

void write(const std::string& type, const Store& vpdStore,
           const std::string& path)
{
    // Get the enum corresponding to type, and call
    // appropriate write FRU method.

    std::string fru = type;
    std::transform(fru.begin(), fru.end(), fru.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    auto iterator = supportedFrus.find(fru);
    if (supportedFrus.end() == iterator)
    {
        throw std::runtime_error("Unsupported FRU: " + std::move(fru));
    }
    else
    {
        switch (iterator->second)
        {
            case Fru::BMC:
            {
                writeFru<Fru::BMC>(vpdStore, path);
                break;
            }

            case Fru::ETHERNET:
            {
                writeFru<Fru::ETHERNET>(vpdStore, path);
                break;
            }

            case Fru::ETHERNET1:
            {
                writeFru<Fru::ETHERNET1>(vpdStore, path);
                break;
            }

            default:
                break;
        }
    }
}

} // namespace inventory
} // namespace vpd
} // namespace openpower
