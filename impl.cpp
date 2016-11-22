#include <exception>
#include <iostream>
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
    if(_vpd.empty() || (lengths::RECORD_MIN > _vpd.size()))
    {
        throw std::runtime_error("Malformed VPD");
    }
    else
    {
        auto buf = _vpd.data() + offsets::VHDR;
        std::string record(buf, lengths::RECORD_NAME);
        if("VHDR" != record)
        {
            throw std::runtime_error("VHDR record not found");
        }
    }
}

} // namespace parser
} // namespace vpd
} // namespace openpower
