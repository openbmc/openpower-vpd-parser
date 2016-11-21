#pragma once

#include <string>
#include <unordered_map>
#include <defines.hpp>
#include <types.hpp>

namespace openpower
{
namespace vpd
{
namespace internal
{

/** @brief Parsed VPD is represented as a dictionary of records, where
 *         each record in itself is a dictionary of keywords */
using Parsed =
    std::unordered_map<std::string,
        std::unordered_map<std::string, std::string>>;

} // namespace internal

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
         *  @param[in] vpd - A parsed VPD object
         */
        explicit Store(internal::Parsed&& vpd): _vpd(std::move(vpd)) {}

        /** @brief Retrives VPD from Store
         *
         *  @tparam R - VPD record
         *  @tparam K - VPD keyword
         *  @returns VPD stored in input record:keyword
         */
        template<Record R, record::Keyword K>
        inline decltype(auto) get() const;

    private:
        /** @brief The store for parsed VPD */
        internal::Parsed _vpd;
};

} // namespace vpd
} // namespace openpower
