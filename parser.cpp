#include <parser.hpp>
#include <impl.hpp>
#include <exception>
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

} // namespace vpd
} // namespace openpower
