#include "parser.hpp"

#include "impl.hpp"

namespace openpower
{
namespace vpd
{

Stores parse(Binary&& vpd)
{
    parser::Impl p(std::move(vpd));
    Stores s = p.run();
    return s;
}

} // namespace vpd
} // namespace openpower
