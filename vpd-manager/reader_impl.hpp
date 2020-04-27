#pragma once

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
 *
 *  A parsed vpd inventory json file is required to construct the class.
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
}; // class ReaderImpl

} // namespace reader
} // namespace manager
} // namespace vpd
} // namespace openpower