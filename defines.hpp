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
    OSYS, /**< Information specific to a system board */
    VNDR, /**< The Vendor Data Record keyword */
    VMSC, /**< The miscellaneous VPD data */
    VRTN  /**< The return data */
};

/** @brief Convert VPD Record name from enum to string
 *  @tparam R - VPD Record
 *  @returns string representation of Record name
 */
template <Record R>
constexpr const char* getRecord() = delete;

template <>
constexpr const char* getRecord<Record::VINI>()
{
    return "VINI";
}

template <>
constexpr const char* getRecord<Record::OPFR>()
{
    return "OPFR";
}

template <>
constexpr const char* getRecord<Record::OSYS>()
{
    return "OSYS";
}

template<>
constexpr const char* getRecord<Record::VNDR>()
{
    return "VNDR";
}

template<>
constexpr const char* getRecord<Record::VMSC>()
{
    return "VMSC";
}

template<>
constexpr const char* getRecord<Record::VRTN>()
{
    return "VRTN";
}

namespace record
{

/** @brief OpenPOWER VPD keywords we're interested in */
enum class Keyword
{
    DR, /**< FRU name/description */
    PN, /**< FRU part number */
    SN, /**< FRU serial number */
    CC, /**< Customer Card Identification Number (CCIN) */
    HW, /**< FRU version */
    B1, /**< MAC Address */
    VN, /**< FRU manufacturer name */
    MB, /**< FRU manufacture date */
    MM, /**< FRU model */
    UD, /**< System UUID */
    VS, /**< OpenPower serial number */
    VP, /**< OpenPower part number */
    RT, /**< The Record Type keyword */
    CE, /**< Card identification number (CCIN) extension */
    VZ, /**< Overall VPD version; VPD change history */
    FN, /**< Card FRU number */
    HE, /**< Hardware EC version */
    CT, /**< Card type */
    B3, /**< Customer card identification number (CCIN)*/
    B4, /**< The manufacturing FRU control (MFC)*/
    B7, /**< Test-specific card information*/
    VD, /**< Record version*/
    IN, /**< Vendor specific data*/
    S0, /**< The S0 keyword */
    I2  /**< Additional vendor specific data*/
};

/** @brief Convert VPD Keyword name from enum to string
 *  @tparam K - VPD Keyword
 *  @returns string representation of Keyword name
 */
template <Keyword K>
constexpr const char* getKeyword() = delete;

template <>
constexpr const char* getKeyword<Keyword::DR>()
{
    return "DR";
}

template <>
constexpr const char* getKeyword<Keyword::PN>()
{
    return "PN";
}

template <>
constexpr const char* getKeyword<Keyword::SN>()
{
    return "SN";
}

template <>
constexpr const char* getKeyword<Keyword::CC>()
{
    return "CC";
}

template <>
constexpr const char* getKeyword<Keyword::HW>()
{
    return "HW";
}

template <>
constexpr const char* getKeyword<Keyword::B1>()
{
    return "B1";
}

template <>
constexpr const char* getKeyword<Keyword::VN>()
{
    return "VN";
}

template <>
constexpr const char* getKeyword<Keyword::MB>()
{
    return "MB";
}

template <>
constexpr const char* getKeyword<Keyword::MM>()
{
    return "MM";
}

template <>
constexpr const char* getKeyword<Keyword::UD>()
{
    return "UD";
}

template <>
constexpr const char* getKeyword<Keyword::VS>()
{
    return "VS";
}

template <>
constexpr const char* getKeyword<Keyword::VP>()
{
    return "VP";
}

template<>
constexpr const char* getKeyword<Keyword::RT>()
{
    return "RT";
}

template<>
constexpr const char* getKeyword<Keyword::CE>()
{
    return "CE";
}

template<>
constexpr const char* getKeyword<Keyword::VZ>()
{
    return "VZ";
}

template<>
constexpr const char* getKeyword<Keyword::FN>()
{
    return "FN";
}

template<>
constexpr const char* getKeyword<Keyword::HE>()
{
    return "HE";
}

template<>
constexpr const char* getKeyword<Keyword::CT>()
{
    return "CT";
}

template<>
constexpr const char* getKeyword<Keyword::B3>()
{
    return "B3";
}

template<>
constexpr const char* getKeyword<Keyword::B4>()
{
    return "B4";
}

template<>
constexpr const char* getKeyword<Keyword::B7>()
{
    return "B7";
}

template<>
constexpr const char* getKeyword<Keyword::VD>()
{
    return "VD";
}

template<>
constexpr const char* getKeyword<Keyword::IN>()
{
    return "IN";
}

template<>
constexpr const char* getKeyword<Keyword::S0>()
{
    return "S0";
}

template<>
constexpr const char* getKeyword<Keyword::I2>()
{
    return "I2";
}

} // namespace record

/** @brief FRUs whose VPD we're interested in
 *
 *  BMC       The VPD on the BMC planar, for eg
 *  ETHERNET  The ethernet card on the BMC
 *  ETHERNET1 The 2nd ethernet card on the BMC
 */
enum Fru
{
    BMC,
    ETHERNET,
    ETHERNET1
};

} // namespace vpd
} // namespace openpower
