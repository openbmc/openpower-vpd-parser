#include <com/ibm/VPD/error.hpp>

namespace sdbusplus
{
namespace com
{
namespace ibm
{
namespace VPD
{
namespace Error
{
const char* LocationNotFound::name() const noexcept
{
    return errName;
}
const char* LocationNotFound::description() const noexcept
{
    return errDesc;
}
const char* LocationNotFound::what() const noexcept
{
    return errWhat;
}
const char* NodeNotFound::name() const noexcept
{
    return errName;
}
const char* NodeNotFound::description() const noexcept
{
    return errDesc;
}
const char* NodeNotFound::what() const noexcept
{
    return errWhat;
}
const char* PathNotFound::name() const noexcept
{
    return errName;
}
const char* PathNotFound::description() const noexcept
{
    return errDesc;
}
const char* PathNotFound::what() const noexcept
{
    return errWhat;
}
const char* RecordNotFound::name() const noexcept
{
    return errName;
}
const char* RecordNotFound::description() const noexcept
{
    return errDesc;
}
const char* RecordNotFound::what() const noexcept
{
    return errWhat;
}
const char* KeywordNotFound::name() const noexcept
{
    return errName;
}
const char* KeywordNotFound::description() const noexcept
{
    return errDesc;
}
const char* KeywordNotFound::what() const noexcept
{
    return errWhat;
}

} // namespace Error
} // namespace VPD
} // namespace ibm
} // namespace com
} // namespace sdbusplus
