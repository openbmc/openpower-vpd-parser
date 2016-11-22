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
    RECORD = 4
};

}

void Impl::hasHeader() const
{
    auto buf = _vpd.data() + offsets::VHDR;
    std::string record(buf, lengths::RECORD);
    if("VHDR" != record)
    {
        throw std::runtime_error("VHDR record not found");
    }
}

} // namespace parser
} // namespace vpd
} // namespace openpower
