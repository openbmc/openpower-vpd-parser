#pragma once

#include "const.hpp"
#include "store.hpp"
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
/** @brief API to obtain a dictionary of path -> services
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
 * @param[in] additionalData - Map holding the additional data
 * @param[in] sev - Severity
 * @param[in] errIntf - error interface
 */
void createPEL(const std::map<std::string, std::string>& additionalData,
               const constants::PelSeverity& sev, const std::string& errIntf);

/**
 * @brief getVpdFilePath
 * Get vpd file path corresponding to the given object path.
 * @param[in] - json file path
 * @param[in] - Object path
 * @return - Vpd file path
 */
inventory::VPDfilepath getVpdFilePath(const string& jsonFile,
                                      const std::string& ObjPath);

/**
 * @brief isPathInJson
 * API which checks for the presence of the given eeprom path in the given json.
 * @param[in] - eepromPath
 * @return - true if the eeprom is present in the json; false otherwise
 */
bool isPathInJson(const std::string& eepromPath);

/**
 * @brief isRecKwInDbusJson
 * API which checks whether the given keyword under the given record is to be
 * published on dbus or not. Checks against the keywords present in
 * dbus_property.json.
 * @param[in] - record name
 * @param[in] - keyword name
 * @return - true if the record-keyword pair is present in dbus_property.json;
 * false otherwise.
 */
bool isRecKwInDbusJson(const std::string& record, const std::string& keyword);

/**
 * @brief Check the type of VPD.
 *
 * Checks the type of vpd based on the start tag.
 * @param[in] vector - Vpd data in vector format
 *
 * @return enum of type vpdType
 */
constants::vpdType vpdTypeCheck(const Binary& vector);

/*
 * @brief This method does nothing. Just an empty function to return null
 * at the end of variadic template args
 */
inline string getCommand()
{
    return "";
}

/**
 * @brief This function to arrange all arguments to make commandy
 * @param[in] arguments to create the command
 * @return cmd - command string
 */
template <typename T, typename... Types>
inline string getCommand(T arg1, Types... args)
{
    string cmd = " " + arg1 + getCommand(args...);

    return cmd;
}

/**
 * @brief This API takes arguments, creates a shell command line and executes
 * them.
 * @param[in] arguments for command
 * @returns output of that command
 */
template <typename T, typename... Types>
inline vector<string> executeCmd(T&& path, Types... args)
{
    vector<string> stdOutput;
    array<char, 128> buffer;

    string cmd = path + getCommand(args...);

    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
    {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        stdOutput.emplace_back(buffer.data());
    }

    return stdOutput;
}

/** @brief This API checks for IM and HW keywords, and based
 *         on these values decides which system json to be used.
 *  @param[in] vpdMap -  parsed vpd
 *  @returns System json path
 */
string getSystemsJson(const Parsed& vpdMap);

/** @brief Reads HW Keyword from the vpd
 *  @param[in] vpdMap -  parsed vpd
 *  @returns value of HW Keyword
 */
const string getHW(const Parsed& vpdMap);

/** @brief Reads IM Keyword from the vpd
 *  @param[in] vpdMap -  parsed vpd
 *  @returns value of IM Keyword
 */
const string getIM(const Parsed& vpdMap);

/** @brief Translate udev event generated path to a generic /sys/bus eeprom path
 *  @param[io] file - path generated from udev event.
 */
void udevToGenericPath(string& file);

/**
 * @brief API to generate a vpd name in some pattern.
 * This vpd-name denotes name of the bad vpd file.
 * For i2c eeproms - the pattern of the vpd-name will be
 * i2c-<bus-number>-<eeprom-address>. For spi eeproms - the pattern of the
 * vpd-name will be spi-<spi-number>.
 *
 * @param[in] file - file path of the vpd
 * @return the vpd-name.
 */
string getBadVpdName(const string& file);

/**
 * @brief API which dumps the broken/bad vpd in a directory
 * When the vpd is bad, this api places  the bad vpd file inside
 * "/tmp/bad-vpd" in BMC, in order to collect bad VPD data as a part of user
 * initiated BMC dump.
 *
 * @param[in] file - bad vpd file path
 * @param[in] vpdVector - bad vpd vector
 */
void dumpBadVpd(const std::string& file, const Binary& vpdVector);

/*
 * @brief This function fetches the value for given keyword in the given record
 *        from vpd data and returns this value.
 *
 * @param[in] vpdMap - vpd to find out the data
 * @param[in] rec - Record under which desired keyword exists
 * @param[in] kwd - keyword to read the data from
 *
 * @returns keyword value if record/keyword combination found
 *          empty string if record or keyword is not found.
 */
const string getKwVal(const Parsed& vpdMap, const string& rec,
                      const string& kwd);

/** @brief This creates a complete command using all it's input parameters,
 *         to bind or unbind the driver.
 *  @param[in] devNameAddr - device address on that bus
 *  @param[in] busType - i2c, spi
 *  @param[in] driverType - type of driver like at24
 *  @param[in] bindOrUnbind - either bind or unbind
 *  @returns  Command to bind or unbind the driver.
 */
inline string createBindUnbindDriverCmnd(const string& devNameAddr,
                                         const string& busType,
                                         const string& driverType,
                                         const string& bindOrUnbind)
{
    return ("echo " + devNameAddr + " > /sys/bus/" + busType + "/drivers/" +
            driverType + "/" + bindOrUnbind);
}

} // namespace vpd
} // namespace openpower