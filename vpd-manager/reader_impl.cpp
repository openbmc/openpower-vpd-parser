#include "config.h"

#include "reader_impl.hpp"

#include "ibm_vpd_utils.hpp"

#include <com/ibm/VPD/error.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <algorithm>
#include <map>
#include <vector>

#ifdef ManagerTest
#include "reader_test.hpp"
#endif

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
using namespace openpower::vpd::utils::interface;

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
    const LocationCode& locationCode, const NodeNumber& nodeNumber,
    const LocationCodeMap& frusLocationCode) const
{
    // unused at this moment. Hence to avoid warnings
    (void)nodeNumber;
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

    std::string expandedLocationCode{};
#ifndef ManagerTest
    utility utilObj;
#endif
    expandedLocationCode = utilObj.readBusProperty(
        iterator->second, IBM_LOCATION_CODE_INF, "LocationCode");
    return expandedLocationCode;
}

ListOfPaths ReaderImpl::getFrusAtLocation(
    const LocationCode& locationCode, const NodeNumber& nodeNumber,
    const LocationCodeMap& frusLocationCode) const
{
    // unused at this moment, to avoid compilation warning
    (void)nodeNumber;

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
                 inventoryPaths.push_back(INVENTORY_PATH + mappedItem.second);
             });
    return inventoryPaths;
}

std::tuple<LocationCode, NodeNumber>
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

    std::string fc{};
#ifndef ManagerTest
    utility utilObj;
#endif

    fc = utilObj.readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VCEN", "FC");

    // get the first part of expanded location code to check for FC or TM
    std::string firstKeyword = locationCode.substr(1, 4);

    LocationCode unexpandedLocationCode{};
    NodeNumber nodeNummber = INVALID_NODE_NUMBER;

    // check if this value matches the value of FC kwd
    if (fc.substr(0, 4) ==
        firstKeyword) // implies this is Ufcs format location code
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
        std::string tm{};
        // read TM kwd value
        tm =
            utilObj.readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VSYS", "TM");
        ;

        // check if the substr matches to TM kwd
        if (tm.substr(0, 4) ==
            firstKeyword) // implies this is Umts format of location code
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

ListOfPaths ReaderImpl::getFRUsByExpandedLocationCode(
    const inventory::LocationCode& locationCode,
    const inventory::LocationCodeMap& frusLocationCode) const
{
    std::tuple<LocationCode, NodeNumber> locationAndNodePair =
        getCollapsedLocationCode(locationCode);

    return getFrusAtLocation(std::get<0>(locationAndNodePair),
                             std::get<1>(locationAndNodePair),
                             frusLocationCode);
}
} // namespace reader
} // namespace manager
} // namespace vpd
} // namespace openpower
