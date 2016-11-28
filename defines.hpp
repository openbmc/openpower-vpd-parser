#pragma once

namespace openpower
{
namespace vpd
{

/** @brief OpenPOWER VPD records we're interested in
 *
 *  VINI   Initial information, common to all OpenPOWER FRUs
 *  OPFR   OpenPOWER FRU information, common to all OpenPOWER FRUs
 *  OSYS   Information specific to a system board
 */
enum class Record
{
    VINI,
    OPFR,
    OSYS
};

namespace record
{

/** @brief OpenPOWER VPD keywords we're interested in
 *
 *  DR     FRU name/description
 *  PN     FRU part number
 *  SN     FRU serial number
 *  CC     Customer Card Identification Number (CCIN)
 *  HW     FRU version
 *  B1     Custom manufacturing information about a FRU
 *  VN     FRU manufacturer name
 *  MB     FRU manufacture date
 *  MM     FRU model
 */
enum class Keyword
{
    DR,
    PN,
    SN,
    CC,
    HW,
    B1,
    VN,
    MB,
    MM
};

} // namespeace record

/** @brief FRUs whose VPD we're interested in
 *
 *  BMC    The VPD on the BMC planar, for eg
 */
enum Fru
{
    BMC
};

} // namespace vpd
} // namespace openpower
