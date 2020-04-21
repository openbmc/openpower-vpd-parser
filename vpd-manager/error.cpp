#include <xyz/openbmc_project/Common/error.hpp>

namespace sdbusplus
{
namespace xyz
{
namespace openbmc_project
{
namespace Common
{
namespace Error
{
const char* Timeout::name() const noexcept
{
    return errName;
}
const char* Timeout::description() const noexcept
{
    return errDesc;
}
const char* Timeout::what() const noexcept
{
    return errWhat;
}
const char* InternalFailure::name() const noexcept
{
    return errName;
}
const char* InternalFailure::description() const noexcept
{
    return errDesc;
}
const char* InternalFailure::what() const noexcept
{
    return errWhat;
}
const char* InvalidArgument::name() const noexcept
{
    return errName;
}
const char* InvalidArgument::description() const noexcept
{
    return errDesc;
}
const char* InvalidArgument::what() const noexcept
{
    return errWhat;
}
const char* InsufficientPermission::name() const noexcept
{
    return errName;
}
const char* InsufficientPermission::description() const noexcept
{
    return errDesc;
}
const char* InsufficientPermission::what() const noexcept
{
    return errWhat;
}
const char* NotAllowed::name() const noexcept
{
    return errName;
}
const char* NotAllowed::description() const noexcept
{
    return errDesc;
}
const char* NotAllowed::what() const noexcept
{
    return errWhat;
}
const char* NoCACertificate::name() const noexcept
{
    return errName;
}
const char* NoCACertificate::description() const noexcept
{
    return errDesc;
}
const char* NoCACertificate::what() const noexcept
{
    return errWhat;
}
const char* TooManyResources::name() const noexcept
{
    return errName;
}
const char* TooManyResources::description() const noexcept
{
    return errDesc;
}
const char* TooManyResources::what() const noexcept
{
    return errWhat;
}
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
} // namespace Common
} // namespace openbmc_project
} // namespace xyz
} // namespace sdbusplus
