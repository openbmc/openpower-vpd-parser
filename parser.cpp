#include "parser.hpp"

#include "impl.hpp"

namespace openpower
{
namespace vpd
{

Store parse(Binary&& vpd)
{
    parser::Impl p(std::move(vpd));
    Store s = p.run();
    return s;
}

namespace keyword
{
namespace editor
{

std::size_t processHeaderAndTOC(Binary&& vpd, Binary::const_iterator& iterator)
{
    openpower::vpd::parser::Impl p(std::move(vpd));
    std::size_t ptLength = p.processVPD(iterator);
    return ptLength;
}

Binary updateECC(Binary&& vpd, Binary::const_iterator iterator)
{
    openpower::vpd::parser::Impl p(std::move(vpd));
    // update the ECC for the record
    return p.updateRecordECC(iterator);
}

} // namespace editor
} // namespace keyword

} // namespace vpd
} // namespace openpower
