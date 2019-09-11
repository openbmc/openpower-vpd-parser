#pragma once

#include "defines.hpp"
#include "types.hpp"

#include <string>
#include <unordered_map>

namespace openpower
{
namespace vpd
{

/** @brief Parsed VPD is represented as a dictionary of records, where
 *         each record in itself is a dictionary of keywords */
using Parsed = std::unordered_map<std::string,
                                  std::unordered_map<std::string, std::string>>;

/** @class Store
 *  @brief Store for parsed OpenPOWER VPD
 *
 *  A Store object stores parsed OpenPOWER VPD, and provides access
 *  to the VPD, specified by record and keyword. Parsed VPD is typically
 *  provided by the Parser class.
 */
class Store final
{
  public:
    Store() = delete;
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    Store(Store&&) = default;
    Store& operator=(Store&&) = default;
    ~Store() = default;

    /** @brief Construct a Store
     *
     *  @param[in] vpdBuffer - A parsed VPD object
     */
    explicit Store(Parsed&& vpdBuffer) : vpd(std::move(vpdBuffer))
    {
    }

    /** @brief Retrieves VPD from Store as a Parsed object
     *
     *  @returns VPD as a Parsed object
     */
    inline const Parsed& getVpdMap() const
    {
        return vpd;
    }

    /** @brief Retrieves VPD from Store
     *
     *  @tparam R - VPD record
     *  @tparam K - VPD keyword
     *  @returns VPD stored in input record:keyword
     */
    template <Record R, record::Keyword K>
    inline const std::string& get() const;

    /** @brief Checks if VPD exists in store
     *
     *  @tparam R - VPD record
     *  @tparam K - VPD keyword
     *  @returns true if {R,K} exists
     */
    template <Record R, record::Keyword K>
    bool exists() const
    {
        static const std::string record = getRecord<R>();
        static const std::string keyword = record::getKeyword<K>();
        return vpd.count(record) && vpd.at(record).count(keyword);
    }

  private:
    /** @brief The store for parsed VPD */
    Parsed vpd;
};

template <Record R, record::Keyword K>
inline const std::string& Store::get() const
{
    static const std::string record = getRecord<R>();
    static const std::string keyword = record::getKeyword<K>();
    static const std::string empty = "";
    auto kw = vpd.find(record);
    if (vpd.end() != kw)
    {
        auto value = (kw->second).find(keyword);
        if ((kw->second).end() != value)
        {
            return value->second;
        }
    }
    return empty;
}

} // namespace vpd
} // namespace openpower
