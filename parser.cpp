#include "parser.hpp"

#include "impl.hpp"
#include <iostream>
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

std::size_t processHeaderAndTOC(Binary& vpd, Binary::const_iterator& iterator)
{
    openpower::vpd::parser::Impl p(std::move(vpd));
    std::size_t ptLength = p.processVPD(iterator);
    return ptLength;
}

}//namespace keyword
}//namespace editor

} // namespace vpd
} // namespace openpower
