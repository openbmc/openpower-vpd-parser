#include "reader_impl.hpp"

#include "utils.hpp"

#include <algorithm>
#include <com/ibm/VPD/error.hpp>
#include <map>
#include <phosphor-logging/elog-errors.hpp>
#include <vector>
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
using LocationNotFound = sdbusplus::com::ibm::VPD::Error::LocationNotFound;

bool ReaderImpl::isValidLocationCode(const LocationCode& locationCode) const
{
    if ((locationCode.length() < UNEXP_LOCATION_CODE_MIN_LENGTH) ||
        (locationCode[0] != 'U') ||
        ((locationCode.find("fcs", 1, 3) == std::string::npos) &&
         (locationCode.find("mts", 1, 3) == std::string::npos)))
    {
        return false;
    }

    return true;
}

LocationCode ReaderImpl::getExpandedLocationCode(
    const LocationCode& locationCode, const Node& nodeNumber,
    const LocationCodeMap& frusLocationCode) const
{
    if (!isValidLocationCode(locationCode))
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

ListOfPaths
    ReaderImpl::getFrusAtLocation(const LocationCode& locationCode,
                                  const Node& nodeNumber,
                                  const LocationCodeMap& frusLocationCode) const
{
    // TODO:Implementation related to node number
    if (!isValidLocationCode(locationCode))
    {
        // argument is not valid
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("LOCATIONCODE"),
                              Argument::ARGUMENT_VALUE(locationCode.c_str()));
    }

    auto range = frusLocationCode.equal_range(locationCode);

    if (range.first == frusLocationCode.end())
    {
        // TODO: Implementation of error logic till then throwing invalid
        // argument
        // the location code was not found in the system
        // elog<LocationNotFound>();
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("LOCATIONCODE"),
                              Argument::ARGUMENT_VALUE(locationCode.c_str()));
    }

    ListOfPaths inventoryPaths;

    for_each(range.first, range.second,
             [&inventoryPaths](
                 const inventory::LocationCodeMap::value_type& mappedItem) {
                 inventoryPaths.push_back(mappedItem.second);
             });
    return inventoryPaths;
}

std::tuple<LocationCode, Node>
    ReaderImpl::getCollapsedLocationCode(const LocationCode& locationCode) const
{
    // Location code should always start with U and fulfil minimum length
    // criteria.
    if (locationCode[0] != 'U' ||
        locationCode.length() < EXP_LOCATIN_CODE_MIN_LENGTH)
    {
        elog<InvalidArgument>(Argument::ARGUMENT_NAME("LOCATIONCODE"),
                              Argument::ARGUMENT_VALUE(locationCode.c_str()));
    }

    std::string fc =
        readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VCEN", "FC");

    // get the first part of expanded location code to check for FC or TM
    std::string FCorTM = locationCode.substr(1, 4);

    LocationCode unexpandedLocationCode{};
    Node nodeNummber = INVALID_NODE_NUMBER;

    // check if this value matches the value of FC kwd
    if (fc.substr(0, 4) == FCorTM) // implies this is Ufcs format location code
    {
        // period(.) should be there in expanded location code to seggregate FC,
        // Node number and SE values.
        size_t nodeStartPos = locationCode.find('.');
        if (nodeStartPos == std::string::npos)
        {
            elog<InvalidArgument>(
                Argument::ARGUMENT_NAME("LOCATIONCODE"),
                Argument::ARGUMENT_VALUE(locationCode.c_str()));
        }

        // second period(.) should be there to end the node details in non
        // system location code
        size_t nodeEndPos = locationCode.find('.', nodeStartPos + 1);
        if (nodeEndPos == std::string::npos)
        {
            elog<InvalidArgument>(
                Argument::ARGUMENT_NAME("LOCATIONCODE"),
                Argument::ARGUMENT_VALUE(locationCode.c_str()));
        }

        // skip 3 for '.ND'
        nodeNummber = std::stoi(locationCode.substr(
            nodeStartPos + 3, (nodeEndPos - nodeStartPos - 3)));

        // confirm if there are other details apart FC, Node number and SE in
        // location code
        if (locationCode.length() > EXP_LOCATIN_CODE_MIN_LENGTH)
        {
            unexpandedLocationCode =
                locationCode[0] + (std::string) "fcs" +
                locationCode.substr(nodeEndPos + 1 + SE_KWD_LENGTH,
                                    std::string::npos);
        }
        else
        {
            unexpandedLocationCode = "Ufcs";
        }
    }
    else
    {
        // read TM kwd value
        std::string tm =
            readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VSYS", "TM");

        // check if the substr matches to TM kwd
        if (tm.substr(0, 4) ==
            FCorTM) // implies this is Umts format of location code
        {
            // system location code will not have any other details and node
            // number
            unexpandedLocationCode = "Umts";
        }
        // it does not belong to either "fcs" or "mts"
        else
        {
            elog<InvalidArgument>(
                Argument::ARGUMENT_NAME("LOCATIONCODE"),
                Argument::ARGUMENT_VALUE(locationCode.c_str()));
        }
    }

    return std::make_tuple(unexpandedLocationCode, nodeNummber);
}
} // namespace reader
} // namespace manager
} // namespace vpd
} // namespace openpower
