#pragma once

namespace openpower
{
namespace vpd
{

/** @brief OpenPOWER VPD records we're interested in */
enum class Record
{
    VINI, /**< Initial information, common to all OpenPOWER FRUs */
    OPFR, /**< OpenPOWER FRU information, common to all OpenPOWER FRUs */
    OSYS  /**< Information specific to a system board */
};

/** @brief Convert VPD Record name from enum to string
 *  @tparam R - VPD Record
 *  @returns string representation of Record name
 */
template<Record R>
constexpr const char* getRecord();

template<>
constexpr const char* getRecord<Record::VINI>()
{
    return "VINI";
}

template<>
constexpr const char* getRecord<Record::OPFR>()
{
    return "OPFR";
}

template<>
constexpr const char* getRecord<Record::OSYS>()
{
    return "OSYS";
}

namespace record
{

/** @brief OpenPOWER VPD keywords we're interested in */
enum class Keyword
{
    DR,  /**< FRU name/description */
    PN,  /**< FRU part number */
    SN,  /**< FRU serial number */
    CC,  /**< Customer Card Identification Number (CCIN) */
    HW,  /**< FRU version */
    B1,  /**< MAC Address */
    VN,  /**< FRU manufacturer name */
    MB,  /**< FRU manufacture date */
    MM   /**< FRU model */
};

/** @brief Convert VPD Keyword name from enum to string
 *  @tparam K - VPD Keyword
 *  @returns string representation of Keyword name
 */
template<Keyword K>
constexpr const char* getKeyword();

template<>
constexpr const char* getKeyword<Keyword::DR>()
{
    return "DR";
}

template<>
constexpr const char* getKeyword<Keyword::PN>()
{
    return "PN";
}

template<>
constexpr const char* getKeyword<Keyword::SN>()
{
    return "SN";
}

template<>
constexpr const char* getKeyword<Keyword::CC>()
{
    return "CC";
}

template<>
constexpr const char* getKeyword<Keyword::HW>()
{
    return "HW";
}

template<>
constexpr const char* getKeyword<Keyword::B1>()
{
    return "B1";
}

template<>
constexpr const char* getKeyword<Keyword::VN>()
{
    return "VN";
}

template<>
constexpr const char* getKeyword<Keyword::MB>()
{
    return "MB";
}

template<>
constexpr const char* getKeyword<Keyword::MM>()
{
    return "MM";
}

} // namespace record
} // namespace vpd
} // namespace openpower
