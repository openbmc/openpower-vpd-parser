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

/** @brief Api to Get d-bus service for given interface
 *  @param[in] - Bus object
 *  @param[in] - object path of the service
 *  @param[in] - interface under the object path
 *  @return service name
 */
std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface);

/** @brief Call inventory-manager to add objects
 *
 *  @param [in] objects - Map of inventory object paths
 */
void callPIM(ObjectMap&& objects);

/** @brief Api to obtain a dictionary of path -> services
 * where path is in subtree and services is of the type
 * returned by the GetObject method.
 *
 * @param [in] root - Root path for object subtree
 * @param [in] depth - Maximum subtree depth required
 * @param [in] interfaces - Array to interfaces for which
 * result is required.
 * @return A dictionary of Path -> services
 */
MapperResponse
    getObjectSubtreeForInterfaces(const std::string& root, const int32_t depth,
                                  const std::vector<std::string>& interfaces);

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
 * @brief API to create PEL entry
 * @param[in] Map holding the additional data
 * @param[in] error interface
 */
void createPEL(const std::map<std::string, std::string>& additionalData,
               const std::string& errIntf);

void createTestPEL(const std::map<std::string, std::string>& additionalData,
                   const std::string& severity, const std::string& errIntf);

} // namespace vpd
} // namespace openpower
