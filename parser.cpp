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

void processHeader(Binary&& vpd)
{
    parser::Impl p(std::move(vpd));
    p.checkVPDHeader();
}

} // namespace editor
} // namespace keyword

} // namespace vpd
} // namespace openpower
