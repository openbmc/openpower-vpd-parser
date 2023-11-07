#pragma once

#include "const.hpp"
#include "store.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <optional>
#include <variant>

namespace openpower
{
namespace vpd
{

// Map which holds system vpd keywords which can be restored at standby and via
// vpd-tool and also can be used to reset keywords to its defaults at
// manufacturing. The list of keywords for VSYS record is as per the S0 system.
// Should be updated for another type of systems For those keywords whose
// default value is system specific, the default value field is left empty.
// Record : {Keyword, Default value, Is PEL required on restore failure, Is MFG
// reset required, backupVpdRecName, backupVpdKwName}
static const inventory::SystemKeywordsMap svpdKwdMap{
    {"VSYS",
     {inventory::SystemKeywordInfo("BR", Binary(2, 0x20), true, true, "VSBK",
                                   "BR"),
      inventory::SystemKeywordInfo("TM", Binary(8, 0x20), true, true, "VSBK",
                                   "TM"),
      inventory::SystemKeywordInfo("SE", Binary(7, 0x20), true, true, "VSBK",
                                   "SE"),
      inventory::SystemKeywordInfo("SU", Binary(6, 0x20), true, true, "VSBK",
                                   "SU"),
      inventory::SystemKeywordInfo("RB", Binary(4, 0x20), true, true, "VSBK",
                                   "RB"),
      inventory::SystemKeywordInfo("WN", Binary(12, 0x20), true, true, "VSBK",
                                   "WN"),
      inventory::SystemKeywordInfo("RG", Binary(4, 0x20), true, true, "VSBK",
                                   "RG"),
      inventory::SystemKeywordInfo("FV", Binary(32, 0x20), false, true, "VSBK",
                                   "FV")}},
    {"VCEN",
     {inventory::SystemKeywordInfo("FC", Binary(8, 0x20), true, false, "VSBK",
                                   "FC"),
      inventory::SystemKeywordInfo("SE", Binary(7, 0x20), true, true, "VSBK",
                                   "ES")}},
    {"LXR0",
     {inventory::SystemKeywordInfo("LX", Binary(8, 0x00), true, false, "VSBK",
                                   "LX")}},
    {"UTIL",
     {inventory::SystemKeywordInfo("D0", Binary(1, 0x00), true, true, "VSBK",
                                   "D0"),
      inventory::SystemKeywordInfo("D1", Binary(1, 0x00), false, true, "VSBK",
                                   "D1"),
      inventory::SystemKeywordInfo("F0", Binary(8, 0x00), false, true, "VSBK",
                                   "F0"),
      inventory::SystemKeywordInfo("F5", Binary(16, 0x00), false, true, "VSBK",
                                   "F5"),
      inventory::SystemKeywordInfo("F6", Binary(16, 0x00), false, true, "VSBK",
                                   "F6")}}};

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
std::string encodeKeyword(const std::string& kw, const std::string& encoding);

/** @brief Reads a property from the inventory manager given object path,
 *         intreface and property.
 *  @param[in] obj - object path
 *  @param[in] inf - interface
 *  @param[in] prop - property whose value is fetched
 *  @return [out] - value of the property
 */
std::string readBusProperty(const std::string& obj, const std::string& inf,
                            const std::string& prop);

/** @brief A templated function to read D-Bus properties.
 *
 *  @param[in] service - Service path
 *  @param[in] object - object path
 *  @param[in] inf - interface
 *  @param[in] prop - property whose value is fetched
 *  @return The property value of its own type.
 */
template <typename T>
T readDBusProperty(const std::string& service, const std::string& object,
                   const std::string& inf, const std::string& prop)
{
    T retVal{};
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto properties = bus.new_method_call(service.c_str(), object.c_str(),
                                              "org.freedesktop.DBus.Properties",
                                              "Get");
        properties.append(inf);
        properties.append(prop);
        auto result = bus.call(properties);
        result.read(retVal);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << e.what();
    }
    return retVal;
}

/** @brief A templated method to get all D-Bus properties
 *
 * @param[in] service - Service path
 * @param[in] object - Object path
 * @param[in] inf - Interface
 *
 * @return All properties under the given interface.
 */
template <typename T>
T getAllDBusProperty(const std::string& service, const std::string& object,
                     const std::string& inf)
{
    T retVal{};
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto allProperties =
            bus.new_method_call(service.c_str(), object.c_str(),
                                "org.freedesktop.DBus.Properties", "GetAll");
        allProperties.append(inf);

        auto result = bus.call(allProperties);
        result.read(retVal);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << e.what();
    }
    return retVal;
}

/**
 * @brief API to create PEL entry
 * The api makes synchronous call to phosphor-logging create api.
 * @param[in] additionalData - Map holding the additional data
 * @param[in] sev - Severity
 * @param[in] errIntf - error interface
 */
void createSyncPEL(const std::map<std::string, std::string>& additionalData,
                   const constants::PelSeverity& sev,
                   const std::string& errIntf);

/**
 * @brief Api to create PEL.
 * A wrapper api through which sync/async call to phosphor-logging create api
 * can be made as and when required.
 * sdBus as nullptr will result in sync call else async call will be made with
 * just "DESCRIPTION" key/value pair in additional data.
 * To make asyn call with more fields in additional data call
 * "sd_bus_call_method_async" in place.
 *
 * @param[in] additionalData - Map of additional data.
 * @param[in] sev - severity of the PEL.
 * @param[in] errIntf - Error interface to be used in PEL.
 * @param[in] sdBus - Pointer to Sd-Bus
 */
void createPEL(const std::map<std::string, std::string>& additionalData,
               const constants::PelSeverity& sev, const std::string& errIntf,
               sd_bus* sdBus);

/**
 * @brief getVpdFilePath
 * Get vpd file path corresponding to the given object path.
 * @param[in] - json file path
 * @param[in] - Object path
 * @return - Vpd file path
 */
inventory::VPDfilepath getVpdFilePath(const std::string& jsonFile,
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
inline std::string getCommand()
{
    return "";
}

/**
 * @brief This function to arrange all arguments to make commandy
 * @param[in] arguments to create the command
 * @return cmd - command string
 */
template <typename T, typename... Types>
inline std::string getCommand(T arg1, Types... args)
{
    std::string cmd = " " + arg1 + getCommand(args...);

    return cmd;
}

/**
 * @brief This API takes arguments, creates a shell command line and executes
 * them.
 * @param[in] arguments for command
 * @returns output of that command
 */
template <typename T, typename... Types>
inline std::vector<std::string> executeCmd(T&& path, Types... args)
{
    std::vector<std::string> stdOutput;
    std::array<char, 128> buffer;

    std::string cmd = path + getCommand(args...);

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                  pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
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
std::string getSystemsJson(const Parsed& vpdMap);

/** @brief Reads HW Keyword from the vpd
 *  @param[in] vpdMap -  parsed vpd
 *  @returns value of HW Keyword
 */
const std::string getHW(const Parsed& vpdMap);

/** @brief Reads IM Keyword from the vpd
 *  @param[in] vpdMap -  parsed vpd
 *  @returns value of IM Keyword
 */
const std::string getIM(const Parsed& vpdMap);

/** @brief Translate udev event generated path to a generic /sys/bus eeprom path
 *  @param[io] file - path generated from udev event.
 *  @param[in] driver - kernel driver used by the device.
 */
void udevToGenericPath(std::string& file, const std::string& driver);

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
std::string getBadVpdName(const std::string& file);

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
const std::string getKwVal(const Parsed& vpdMap, const std::string& rec,
                           const std::string& kwd);

/** @brief This creates a complete command using all it's input parameters,
 *         to bind or unbind the driver.
 *  @param[in] devNameAddr - device address on that bus
 *  @param[in] busType - i2c, spi
 *  @param[in] driverType - type of driver like at24
 *  @param[in] bindOrUnbind - either bind or unbind
 *  @returns  Command to bind or unbind the driver.
 */
inline std::string createBindUnbindDriverCmnd(const std::string& devNameAddr,
                                              const std::string& busType,
                                              const std::string& driverType,
                                              const std::string& bindOrUnbind)
{
    return ("echo " + devNameAddr + " > /sys/bus/" + busType + "/drivers/" +
            driverType + "/" + bindOrUnbind);
}

/**
 * @brief Get Printable Value
 *
 * Checks if the value has non printable characters.
 * Returns hex value if non printable char is found else
 * returns ascii value.
 *
 * @param[in] kwVal - Reference of the input data, Keyword value
 * @return printable value - either in hex or in ascii.
 */
std::string getPrintableValue(const std::variant<Binary, std::string>& kwVal);

/**
 * @brief Convert array to hex string.
 * @param[in] kwVal - input data, Keyword value
 * @return hexadecimal string of bytes.
 */
std::string hexString(const std::variant<Binary, std::string>& kwVal);

/**
 * @brief Return presence of the FRU.
 *
 * This API returns the presence information of the FRU corresponding to the
 * given EEPROM. If the JSON contains no information about presence detect, this
 * will return an empty optional. Else it will get the presence GPIO information
 * from the JSON and return the appropriate present status.
 * In case of GPIO find/read errors, it will return false.
 *
 * @param[in] json - The VPD JSON
 * @param[in] file - EEPROM file path
 * @return Empty optional if there is no presence info. Else returns presence
 * based on the GPIO read.
 */
std::optional<bool> isPresent(const nlohmann::json& json,
                              const std::string& file);

/**
 * @brief Performs any pre-action needed to get the FRU setup for
 * collection.
 *
 * @param[in] json - json object
 * @param[in] file - eeprom file path
 * @return - success or failure
 */
bool executePreAction(const nlohmann::json& json, const std::string& file);

/**
 * @brief This API will be called at the end of VPD collection to perform any
 * post actions.
 *
 * @param[in] json - json object
 * @param[in] file - eeprom file path
 */
void executePostFailAction(const nlohmann::json& json, const std::string& file);

/**
 * @brief Helper function to insert or merge in map.
 *
 * This method checks in the given inventory::InterfaceMap if the given
 * interface key is existing or not. If the interface key already exists, given
 * property map is inserted into it. If the key does'nt exist then given
 * interface and property map pair is newly created. If the property present in
 * propertymap already exist in the InterfaceMap, then the new property value is
 * ignored.
 *
 * @param[in,out] map - map object of type inventory::InterfaceMap only.
 * @param[in] interface - Interface name.
 * @param[in] property - new property map that needs to be emplaced.
 */
void insertOrMerge(inventory::InterfaceMap& map,
                   const inventory::Interface& interface,
                   inventory::PropertyMap&& property);

/**
 * @brief Utility API to set a D-Bus property
 *
 * This calls org.freedesktop.DBus.Properties;Set method with the supplied
 * arguments
 *
 * @tparam T Template type of the D-Bus property
 * @param service[in] - The D-Bus service name.
 * @param object[in] - The D-Bus object on which the property is to be set.
 * @param interface[in] - The D-Bus interface to which the property belongs.
 * @param propertyName[in] - The name of the property to set.
 * @param propertyValue[in] - The value of the property.
 */
template <typename T>
void setBusProperty(const std::string& service, const std::string& object,
                    const std::string& interface,
                    const std::string& propertyName,
                    const std::variant<T>& propertyValue)
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(service.c_str(), object.c_str(),
                                          "org.freedesktop.DBus.Properties",
                                          "Set");
        method.append(interface);
        method.append(propertyName);
        method.append(propertyValue);

        bus.call(method);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

/**
 * @brief Reads BIOS Attribute by name.
 *
 * @param attrName[in] - The BIOS attribute name.
 * @return std::variant<int64_t, std::string> - The BIOS attribute value.
 */
std::variant<int64_t, std::string>
    readBIOSAttribute(const std::string& attrName);

/**
 * @brief Returns the power state for chassis0
 * @return The chassis power state.
 */
std::string getPowerState();

/**
 * @brief Reads VPD from the supplied EEPROM
 *
 * This function reads the given VPD EEPROM file and returns its contents as a
 * byte array. It handles any offsets into the file that need to be taken care
 * of by looking up the VPD JSON for a possible offset key.
 *
 * @param js[in] - The VPD JSON Object
 * @param file[in] - The path to the EEPROM to read
 * @return A byte array containing the raw VPD.
 */
Binary getVpdDataInVector(const nlohmann::json& js, const std::string& file);

/**
 * @brief Get D-bus name for the keyword
 * Some of the VPD keywords has different name in PIM when compared with its
 * name from hardware. This method returns the D-bus name for the given keyword.
 *
 * @param[in] keyword - Keyword name
 * @return D-bus name for the keyword
 */
std::string getDbusNameForThisKw(const std::string& keyword);

/**
 * @brief Find backup VPD path if any for the system VPD
 *
 * @param[out] backupEepromPath - Backup VPD path
 * @param[out] backupInvPath - Backup inventory path
 * @param[in] js - Inventory JSON object
 */
void findBackupVPDPaths(std::string& backupEepromPath,
                        std::string& backupInvPath, const nlohmann::json& js);

/**
 * @brief Get backup VPD's record and keyword for the given system VPD keyword
 *
 * @param[in,out] record - Record name
 * @param[in,out] keyword - Keyword name
 */
void getBackupRecordKeyword(std::string& record, std::string& keyword);

} // namespace vpd
} // namespace openpower
