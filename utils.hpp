#pragma once

#include "const.hpp"
#include "types.hpp"

#include <iostream>

using namespace std;
namespace openpower
{
namespace vpd
{
/**
 * @brief Types of VPD
 */
enum vpdType
{
    IPZ_VPD,           /**< IPZ VPD type */
    KEYWORD_VPD,       /**< Keyword VPD type */
    MEMORY_VPD,        /**< Memory VPD type */
    INVALID_VPD_FORMAT /**< Invalid VPD type */
};

/**
 * @brief Check the type of VPD.
 *
 * Checks the type of vpd based on the start tag.
 * @param[in] vector - Vpd data in vector format
 *
 * @return enum of type vpdType
 */
vpdType vpdTypeCheck(const Binary& vector);

/** @brief Return the hex representation of the incoming byte
 *
 * @param [in] c - The input byte
 * @returns The hex representation of the byte as a character.
 */
constexpr auto toHex(size_t c)
{
    constexpr auto map = "0123456789abcdef";
    return map[c];
}

namespace inventory
{

/** @brief Get inventory-manager's d-bus service
 */
auto getPIMService();

/** @brief Call inventory-manager to add objects
 *
 *  @param [in] objects - Map of inventory object paths
 */
void callPIM(ObjectMap&& objects);

} // namespace inventory

/**@brief This API reads 2 Bytes of data and swap the read data
 * @param[in] iterator- Pointer pointing to the data to be read
 * @return returns 2 Byte data read at the given pointer
 */
openpower::vpd::constants::LE2ByteData
    readUInt16LE(Binary::const_iterator iterator);

/** @brief Encodes a keyword for D-Bus.
 *  @param[in] kw - kwd data in string format
 *  @param[in] encoding - required for kwd data
 */
string encodeKeyword(const string& kw, const string& encoding);

/** @brief Reads a property from the inventory manager given object path,
 *         intreface and property.
 *  @param[in] obj - object path
 *  @param[in] inf - interface
 *  @param[in] prop - property whose value is fetched
 *  @return [out] - value of the property
 */
string readBusProperty(const string& obj, const string& inf,
                       const string& prop);
} // namespace vpd
} // namespace openpower
