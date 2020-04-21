#pragma once

#include "types.hpp"

#include <iostream>
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

}; // class ReaderImpl

} // namespace reader
} // namespace manager
} // namespace vpd
} // namespace openpower