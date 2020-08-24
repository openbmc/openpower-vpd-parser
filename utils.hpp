#pragma once

#include "const.hpp"
#include "types.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
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
 *  @param[in] - Bus object
 *  @param[in] - object path of the service
 *  @param[in] - interface under the object pah
 *  @return service name
 */
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

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

/**
 * @brief getVpdFilePath
 * Get vpd file path corresponding to the given object path.
 * @param[in] - jsonFile
 * @param[in] - Object path
 * @return - Vpd file path
 */
inventory::VPDfilepath getVpdFilePath(const json& jsonFile,
                                      const std::string& ObjPath);

/**
 * @brief getSHA
 * API to generate a SHA value for a given string
 * @param[in] - EEPROM path
 * @return - SHA string
 */
std::string getSHA(const std::string& filePath);
namespace filestream
{
/**
 * @brief streamStatus
 * Return the file stream status.
 * @param[in] - vpdFileStream
 *
 * @return - status of the given file stream(eof/fail/bad/good)
 */
std::string streamStatus(std::fstream& vpdFileStream);
} // namespace filestream

/**
 * @brief eepromPresenceInJson
 * API which checks for the presence of the given eeprom path in the given json.
 * @param[in] - eepromPath
 * @param[in] - the inventory json object
 * @return - true if the eeprom is present in the json; false otherwise
 */
bool eepromPresenceInJson(const std::string& eepromPath);

/**
 * @brief recKwPresenceInDbusProp
 * API which checks whether the given keyword under the given record is to be
 * published on dbus or not. Checks against the keywords present in
 * dbus_property.json.
 * @param[in] - record name
 * @param[in] - keyword name
 * @return - true if the record-keyword pair is present in dbus_property.json;
 * false otherwise.
 */
bool recKwPresenceInDbusProp(const std::string& record,
                             const std::string& keyword);

/**
 * @brief Convert hex/ascii values to Binary
 * @param[in] - value in hex/ascii.
 * @param[out] - value in binary.
 */
Binary toBinary(const std::string& value);
} // namespace vpd
} // namespace openpower
