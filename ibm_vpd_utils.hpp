#pragma once

#include "const.hpp"
#include "store.hpp"
#include "types.hpp"

#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>

using namespace std;

namespace openpower
{
namespace vpd
{

// mapping of severity enum to severity interface
static std::unordered_map<constants::PelSeverity, std::string> sevMap = {
    {constants::PelSeverity::INFORMATIONAL,
     "xyz.openbmc_project.Logging.Entry.Level.Informational"},
    {constants::PelSeverity::DEBUG,
     "xyz.openbmc_project.Logging.Entry.Level.Debug"},
    {constants::PelSeverity::NOTICE,
     "xyz.openbmc_project.Logging.Entry.Level.Notice"},
    {constants::PelSeverity::WARNING,
     "xyz.openbmc_project.Logging.Entry.Level.Warning"},
    {constants::PelSeverity::CRITICAL,
     "xyz.openbmc_project.Logging.Entry.Level.Critical"},
    {constants::PelSeverity::EMERGENCY,
     "xyz.openbmc_project.Logging.Entry.Level.Emergency"},
    {constants::PelSeverity::ERROR,
     "xyz.openbmc_project.Logging.Entry.Level.Error"},
    {constants::PelSeverity::ALERT,
     "xyz.openbmc_project.Logging.Entry.Level.Alert"}};

// Map to hold record, kwd pair which can be re-stored at standby.
// The list of keywords for VSYS record is as per the S0 system. Should
// be updated for another type of systems
static const std::unordered_map<std::string, std::vector<std::string>>
    svpdKwdMap{{"VSYS", {"BR", "TM", "SE", "SU", "RB", "WN", "RG"}},
               {"VCEN", {"FC", "SE"}},
               {"LXR0", {"LX"}},
               {"UTIL", {"D0"}}};

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

/**
 * @brief Get Printable Value
 *
 * Checks if the vector value has non printable characters.
 * Returns hex value if non printable char is found else
 * returns ascii value.
 *
 * @param[in] vector - Reference of the Binary vector
 * @return printable value - either in hex or in ascii.
 */
string getPrintableValue(const Binary& vec);

/**
 * @brief Convert byte array to hex string.
 * @param[in] vec - byte array
 * @return hexadecimal string of bytes.
 */
string byteArrayToHexString(const Binary& vec);

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
std::optional<bool> isPresent(const nlohmann::json& json, const string& file);

/**
 * @brief Performs any pre-action needed to get the FRU setup for
 * collection.
 *
 * @param[in] json - json object
 * @param[in] file - eeprom file path
 * @return - success or failure
 */
bool executePreAction(const nlohmann::json& json, const string& file);

/**
 * @brief This API will be called at the end of VPD collection to perform any
 * post actions.
 *
 * @param[in] json - json object
 * @param[in] file - eeprom file path
 */
void executePostFailAction(const nlohmann::json& json, const string& file);

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
        auto method =
            bus.new_method_call(service.c_str(), object.c_str(),
                                "org.freedesktop.DBus.Properties", "Set");
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
} // namespace vpd
} // namespace openpower