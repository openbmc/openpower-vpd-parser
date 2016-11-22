#include <exception>
#include <iostream>
#include <iterator>
#include "impl.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{

namespace offsets
{

enum Offsets
{
    VHDR = 17
};

}

namespace lengths
{

enum Lengths
{
    RECORD_NAME = 4,
    RECORD_MIN = 44,
};

}

void Impl::checkHeader() const
{
    if (vpd.empty() || (lengths::RECORD_MIN > vpd.size()))
    {
        throw std::runtime_error("Malformed VPD");
    }
    else
    {
        auto iterator = vpd.cbegin();
        std::advance(iterator, offsets::VHDR);
        std::string record(iterator, iterator + lengths::RECORD_NAME);
        if ("VHDR" != record)
        {
            throw std::runtime_error("VHDR record not found");
        }
    }
}

} // namespace parser
} // namespace vpd
} // namespace openpower
