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

/** @brief Specialization of VINI::DR */
template<>
inline decltype(auto) Store::get<Record::VINI, record::Keyword::DR>() const
{
    return _vpd.at("VINI").at("DR");
}

/** @brief Specialization of VINI::PN */
template<>
inline decltype(auto) Store::get<Record::VINI, record::Keyword::PN>() const
{
    return _vpd.at("VINI").at("PN");
}

/** @brief Specialization of VINI::SN */
template<>
inline decltype(auto) Store::get<Record::VINI, record::Keyword::SN>() const
{
    return _vpd.at("VINI").at("SN");
}

/** @brief Specialization of VINI::HW */
template<>
inline decltype(auto) Store::get<Record::VINI, record::Keyword::HW>() const
{
    return _vpd.at("VINI").at("HW");
}

/** @brief Specialization of VINI::CC */
template<>
inline decltype(auto) Store::get<Record::VINI, record::Keyword::CC>() const
{
    return _vpd.at("VINI").at("CC");
}

/** @brief Specialization of VINI::B1 */
template<>
inline decltype(auto) Store::get<Record::VINI, record::Keyword::B1>() const
{
    return _vpd.at("VINI").at("B1");
}

/** @brief Specialization of OPFR::VN */
template<>
inline decltype(auto) Store::get<Record::OPFR, record::Keyword::VN>() const
{
    return _vpd.at("OPFR").at("VN");
}

/** @brief Specialization of OPFR::MB */
template<>
inline decltype(auto) Store::get<Record::OPFR, record::Keyword::MB>() const
{
    return _vpd.at("OPFR").at("MB");
}

/** @brief Specialization of OSYS::MM */
template<>
inline decltype(auto) Store::get<Record::OSYS, record::Keyword::MM>() const
{
    return _vpd.at("OSYS").at("MM");
}

} // namespace vpd
} // namespace openpower
