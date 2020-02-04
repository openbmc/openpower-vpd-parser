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

std::tuple<RecordOffset, std::size_t> processHeaderAndTOC(Binary&& vpd)
{
    parser::Impl p(std::move(vpd));
    return p.processVPD();
}

} // namespace editor
} // namespace keyword

} // namespace vpd
} // namespace openpower
