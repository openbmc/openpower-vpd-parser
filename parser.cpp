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

std::size_t processHeaderAndTOC(Binary&& vpd, uint16_t& ptOffset)
{
    parser::Impl p(std::move(vpd));
    std::size_t ptLength = p.processVPD(ptOffset);
    return ptLength;
}

} // namespace editor
} // namespace keyword

} // namespace vpd
} // namespace openpower
