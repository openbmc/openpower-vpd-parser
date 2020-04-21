#include "reader_impl.hpp"

#include "utils.hpp"

#include <com/ibm/vpd/error.hpp>
#include <map>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace openpower
{
namespace vpd
{
namespace manager
{
namespace reader
{

using namespace phosphor::logging;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::constants;

using InvalidArgument =
    sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
using Argument = xyz::openbmc_project::Common::InvalidArgument;
using LocationNotFound = sdbusplus::com::ibm::vpd::Error::LocationNotFound;

std::string ReaderImpl::getExpandedLocationCode(
    const std::string& locationCode, const uint16_t& nodeNumber,
    const LocationCodeMap& frusLocationCode) const
{
    if ((locationCode.length() < UNEXP_LOCATION_CODE_MIN_LENGTH) ||
        ((locationCode.find("fcs", 1, 3) == std::string::npos) &&
         (locationCode.find("mts", 1, 3) == std::string::npos)))
    {
        // argument is not valid
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("LOCATIONCODE"),
                              Argument::ARGUMENT_VALUE(locationCode.c_str()));
    }

    auto iterator = frusLocationCode.find(locationCode);
    if (iterator == frusLocationCode.end())
    {
        // TODO: Implementation of error logic till then throwing invalid
        // argument
        // the location code was not found in the system
        // elog<LocationNotFound>();
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("LOCATIONCODE"),
                              Argument::ARGUMENT_VALUE(locationCode.c_str()));
    }

    std::string expandedLocationCode =
        readBusProperty(iterator->second, LOCATION_CODE_INF, "LocationCode");
    return expandedLocationCode;
}

} // namespace reader
} // namespace manager
} // namespace vpd
} // namespace openpower
