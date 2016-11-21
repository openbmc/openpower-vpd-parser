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

} // namespace record
} // namespace vpd
} // namespace openpower
