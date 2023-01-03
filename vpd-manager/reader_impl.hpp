#pragma once

#include "types.hpp"
#include "utilInterface.hpp"

namespace openpower
{
namespace vpd
{
namespace manager
{

using IUtil = openpower::vpd::utils::interface::UtilityInterface;
/** @class ReaderImpl
 *  @brief Implements functionalities related to reading of VPD related data
 *  from the system.
 */
class ReaderImpl
{
  public:
    ReaderImpl() = default;
    ReaderImpl(const ReaderImpl&) = default;
    ReaderImpl& operator=(const ReaderImpl&) = delete;
    ReaderImpl(ReaderImpl&&) = default;
    ReaderImpl& operator=(ReaderImpl&&) = delete;
    ~ReaderImpl() = default;

#ifdef ManagerTest
    explicit ReaderImpl(IUtil& obj) : utilObj(obj)
    {
    }
#endif

    /** @brief An API to expand a given unexpanded location code.
     *  @param[in] locationCode - unexpanded location code.
     *  @param[in] nodeNumber - node on which we are looking for location code.
     *  @param[in] frusLocationCode - mapping of inventory path and location
     * code.
     *  @return Expanded location code.
     */
    types::LocationCode getExpandedLocationCode(
        const types::LocationCode& locationCode,
        const types::NodeNumber& nodeNumber,
        const types::LocationCodeMap& frusLocationCode) const;

    /** @brief An API to get list of all the FRUs at the given location code
     *  @param[in] - location code in unexpanded format
     *  @param[in] - node number
     *  @param[in] - mapping of location code and Inventory path
     *  @return list of Inventory paths at the given location
     */
    types::ListOfPaths
        getFrusAtLocation(const types::LocationCode& locationCode,
                          const types::NodeNumber& nodeNumber,
                          const types::LocationCodeMap& frusLocationCode) const;

    /** @brief An API to get list of all the FRUs at the given location code
     *  @param[in] - location code in unexpanded format
     *  @param[in] - mapping of location code and Inventory path
     *  @return list of Inventory paths at the given location
     */
    types::ListOfPaths getFRUsByExpandedLocationCode(
        const types::LocationCode& locationCode,
        const types::LocationCodeMap& frusLocationCode) const;

  private:
    /** @brief An api to check validity of location code
     *  @param[in] - location code
     *  @return true/false based on validity check
     */
    bool isValidLocationCode(const types::LocationCode& locationCode) const;

    /** @brief An API to split expanded location code to its un-expanded
     *  format as represented in VPD JSON and the node number.
     *  @param[in] Location code in expanded format.
     *  @return Location code in un-expanded format and its node number.
     */
    std::tuple<types::LocationCode, types::NodeNumber>
        getCollapsedLocationCode(const types::LocationCode& locationCode) const;
#ifdef ManagerTest
    IUtil& utilObj;
#endif

}; // class ReaderImpl

} // namespace manager
} // namespace vpd
} // namespace openpower
