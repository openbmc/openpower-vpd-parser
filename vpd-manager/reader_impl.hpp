#pragma once

#include "types.hpp"

#include <iostream>
#include <sdbusplus/server.hpp>

namespace openpower
{
namespace vpd
{
namespace manager
{
namespace reader
{

/** @class ReaderImpl
 *  @brief Implements functionalities related to reading of VPD related data
 *  from the system.
 */
class ReaderImpl
{
  public:
    ReaderImpl() = default;
    ReaderImpl(const ReaderImpl&) = delete;
    ReaderImpl& operator=(const ReaderImpl&) = delete;
    ReaderImpl(ReaderImpl&&) = delete;
    ReaderImpl& operator=(ReaderImpl&&) = delete;
    ~ReaderImpl() = default;

    /** @brief An api to expand a given unexpanded location code.
     *  @param[in] locationCode - unexpanded location code
     *  @param[in] nodeNumber - node on which we are looking for location code
     *  @return[out] Expanded location code
     */
    std::string getExpandedLocationCode(
        const std::string& locationCode, const uint16_t& nodeNumber,
        const inventory::LocationCodeMap& frusLocationCode);

    /** @brief An api to get list of all the FRUs at the given location code
     *  @param[in] - location code in unexpanded format
     *  @param[in] - node number
     *  @param[in] - mapping of location code and Inventory path
     *  @return[out] - list of Inventory paths at the given location
     */
    std::vector<sdbusplus::message::object_path>
        getFrusAtLocation(const std::string& locationCode,
                          const uint16_t& nodeNumber,
                          const inventory::LocationCodeMap& frusLocationCode);

  private:
    /** @brief An api to check validity of location code
     *  @param[in] - location code
     *  @return[out] - true/false based on validity check
     */
    bool isValidLocationCode(const std::string& locationCode);

}; // class ReaderImpl

} // namespace reader
} // namespace manager
} // namespace vpd
} // namespace openpower