#pragma once

#include "types.hpp"

#include <iostream>

using namespace std;
namespace openpower
{
namespace vpd
{
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

using LE2ByteData = uint16_t;

/**@brief This API reads 2 Bytes of data and swap the read data
 * @param[in] iterator- Pointer pointing to the data to be read
 *
 * @return returns 2 Byte data read at the given pointer
 */
LE2ByteData readUInt16LE(Binary::const_iterator iterator);

/** @brief - converts value to hex
 */
constexpr auto toHex(size_t c);

/** @brief Encodes a keyword for D-Bus.
 *  @param[in] kw - kwd data in string format
 *  @param[in] encoding - required for kwd data
 */
string encodeKeyword(const string& kw, const string& encoding);
} // namespace vpd
} // namespace openpower
