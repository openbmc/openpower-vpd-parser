#include <com/ibm/vpd/error.hpp>

namespace sdbusplus
{
namespace com
{
namespace ibm
{
namespace vpd
{
namespace Error
{
const char* NotFound::name() const noexcept
{
    return errName;
}
const char* NotFound::description() const noexcept
{
    return errDesc;
}
const char* NotFound::what() const noexcept
{
    return errWhat;
}

} // namespace Error
} // namespace vpd
} // namespace ibm
} // namespace com
} // namespace sdbusplus

