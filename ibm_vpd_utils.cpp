#include "config.h"

#include "ibm_vpd_utils.hpp"

#include "common_utility.hpp"
#include "defines.hpp"
#include "vpd_exceptions.hpp"

#include <boost/algorithm/string.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <regex>
#include <sdbusplus/server.hpp>
#include <sstream>
#include <vector>
#include <xyz/openbmc_project/Common/error.hpp>

using json = nlohmann::json;

namespace openpower
{
namespace vpd
{
using namespace openpower::vpd::constants;
using namespace inventory;
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace record;
using namespace openpower::vpd::exceptions;
using namespace common::utility;
using Severity = openpower::vpd::constants::PelSeverity;
namespace fs = std::filesystem;

// mapping of severity enum to severity interface
static std::unordered_map<Severity, std::string> sevMap = {
    {Severity::INFORMATIONAL,
     "xyz.openbmc_project.Logging.Entry.Level.Informational"},
    {Severity::DEBUG, "xyz.openbmc_project.Logging.Entry.Level.Debug"},
    {Severity::NOTICE, "xyz.openbmc_project.Logging.Entry.Level.Notice"},
    {Severity::WARNING, "xyz.openbmc_project.Logging.Entry.Level.Warning"},
    {Severity::CRITICAL, "xyz.openbmc_project.Logging.Entry.Level.Critical"},
    {Severity::EMERGENCY, "xyz.openbmc_project.Logging.Entry.Level.Emergency"},
    {Severity::ERROR, "xyz.openbmc_project.Logging.Entry.Level.Error"},
    {Severity::ALERT, "xyz.openbmc_project.Logging.Entry.Level.Alert"}};

namespace inventory
{

MapperResponse
    getObjectSubtreeForInterfaces(const std::string& root, const int32_t depth,
                                  const std::vector<std::string>& interfaces)
{
    auto bus = sdbusplus::bus::new_default();
    auto mapperCall = bus.new_method_call(mapperDestination, mapperObjectPath,
                                          mapperInterface, "GetSubTree");
    mapperCall.append(root);
    mapperCall.append(depth);
    mapperCall.append(interfaces);

    MapperResponse result = {};

    try
    {
        auto response = bus.call(mapperCall);

        response.read(result);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        log<level::ERR>("Error in mapper GetSubTree",
                        entry("ERROR=%s", e.what()));
    }

    return result;
}

} // namespace inventory

LE2ByteData readUInt16LE(Binary::const_iterator iterator)
{
    LE2ByteData lowByte = *iterator;
    LE2ByteData highByte = *(iterator + 1);
    lowByte |= (highByte << 8);
    return lowByte;
}

/** @brief Encodes a keyword for D-Bus.
 */
string encodeKeyword(const string& kw, const string& encoding)
{
    if (encoding == "MAC")
    {
        string res{};
        size_t first = kw[0];
        res += toHex(first >> 4);
        res += toHex(first & 0x0f);
        for (size_t i = 1; i < kw.size(); ++i)
        {
            res += ":";
            res += toHex(kw[i] >> 4);
            res += toHex(kw[i] & 0x0f);
        }
        return res;
    }
    else if (encoding == "DATE")
    {
        // Date, represent as
        // <year>-<month>-<day> <hour>:<min>
        string res{};
        static constexpr uint8_t skipPrefix = 3;

        auto strItr = kw.begin();
        advance(strItr, skipPrefix);
        for_each(strItr, kw.end(), [&res](size_t c) { res += c; });

        res.insert(BD_YEAR_END, 1, '-');
        res.insert(BD_MONTH_END, 1, '-');
        res.insert(BD_DAY_END, 1, ' ');
        res.insert(BD_HOUR_END, 1, ':');

        return res;
    }
    else // default to string encoding
    {
        return string(kw.begin(), kw.end());
    }
}

string readBusProperty(const string& obj, const string& inf, const string& prop)
{
    std::string propVal{};
    std::string object = INVENTORY_PATH + obj;
    auto bus = sdbusplus::bus::new_default();
    auto properties = bus.new_method_call(
        "xyz.openbmc_project.Inventory.Manager", object.c_str(),
        "org.freedesktop.DBus.Properties", "Get");
    properties.append(inf);
    properties.append(prop);
    auto result = bus.call(properties);
    if (!result.is_method_error())
    {
        variant<Binary, string> val;
        result.read(val);
        if (auto pVal = get_if<Binary>(&val))
        {
            propVal.assign(reinterpret_cast<const char*>(pVal->data()),
                           pVal->size());
        }
        else if (auto pVal = get_if<string>(&val))
        {
            propVal.assign(pVal->data(), pVal->size());
        }
    }
    return propVal;
}

void createPEL(const std::map<std::string, std::string>& additionalData,
               const Severity& sev, const std::string& errIntf)
{
    try
    {
        std::string pelSeverity =
            "xyz.openbmc_project.Logging.Entry.Level.Error";
        auto bus = sdbusplus::bus::new_default();
        auto service = getService(bus, loggerObjectPath, loggerCreateInterface);
        auto method = bus.new_method_call(service.c_str(), loggerObjectPath,
                                          loggerCreateInterface, "Create");

        auto itr = sevMap.find(sev);
        if (itr != sevMap.end())
        {
            pelSeverity = itr->second;
        }

        method.append(errIntf, pelSeverity, additionalData);
        auto resp = bus.call(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        throw std::runtime_error(
            "Error in invoking D-Bus logging create interface to register PEL");
    }
}

inventory::VPDfilepath getVpdFilePath(const string& jsonFile,
                                      const std::string& ObjPath)
{
    ifstream inventoryJson(jsonFile);
    const auto& jsonObject = json::parse(inventoryJson);
    inventory::VPDfilepath filePath{};

    if (jsonObject.find("frus") == jsonObject.end())
    {
        throw(VpdJsonException(
            "Invalid JSON structure - frus{} object not found in ", jsonFile));
    }

    const nlohmann::json& groupFRUS =
        jsonObject["frus"].get_ref<const nlohmann::json::object_t&>();
    for (const auto& itemFRUS : groupFRUS.items())
    {
        const std::vector<nlohmann::json>& groupEEPROM =
            itemFRUS.value().get_ref<const nlohmann::json::array_t&>();
        for (const auto& itemEEPROM : groupEEPROM)
        {
            if (itemEEPROM["inventoryPath"]
                    .get_ref<const nlohmann::json::string_t&>() == ObjPath)
            {
                filePath = itemFRUS.key();
                return filePath;
            }
        }
    }

    return filePath;
}

bool isPathInJson(const std::string& eepromPath)
{
    bool present = false;
    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);

    try
    {
        auto js = json::parse(inventoryJson);
        if (js.find("frus") == js.end())
        {
            throw(VpdJsonException(
                "Invalid JSON structure - frus{} object not found in ",
                INVENTORY_JSON_SYM_LINK));
        }
        json fruJson = js["frus"];

        if (fruJson.find(eepromPath) != fruJson.end())
        {
            present = true;
        }
    }
    catch (json::parse_error& ex)
    {
        throw(VpdJsonException("Json Parsing failed", INVENTORY_JSON_SYM_LINK));
    }
    return present;
}

bool isRecKwInDbusJson(const std::string& recordName,
                       const std::string& keyword)
{
    ifstream propertyJson(DBUS_PROP_JSON);
    json dbusProperty;
    bool present = false;

    if (propertyJson.is_open())
    {
        try
        {
            auto dbusPropertyJson = json::parse(propertyJson);
            if (dbusPropertyJson.find("dbusProperties") ==
                dbusPropertyJson.end())
            {
                throw(VpdJsonException("dbusProperties{} object not found in "
                                       "DbusProperties json : ",
                                       DBUS_PROP_JSON));
            }

            dbusProperty = dbusPropertyJson["dbusProperties"];
            if (dbusProperty.contains(recordName))
            {
                const vector<string>& kwdsToPublish = dbusProperty[recordName];
                if (find(kwdsToPublish.begin(), kwdsToPublish.end(), keyword) !=
                    kwdsToPublish.end()) // present
                {
                    present = true;
                }
            }
        }
        catch (json::parse_error& ex)
        {
            throw(VpdJsonException("Json Parsing failed", DBUS_PROP_JSON));
        }
    }
    else
    {
        // If dbus properties json is not available, we assume the given
        // record-keyword is part of dbus-properties json. So setting the bool
        // variable to true.
        present = true;
    }
    return present;
}

vpdType vpdTypeCheck(const Binary& vpdVector)
{
    // Read first 3 Bytes to check the 11S bar code format
    std::string is11SFormat = "";
    for (uint8_t i = 0; i < FORMAT_11S_LEN; i++)
    {
        is11SFormat += vpdVector[MEMORY_VPD_DATA_START + i];
    }

    if (vpdVector[IPZ_DATA_START] == KW_VAL_PAIR_START_TAG)
    {
        // IPZ VPD FORMAT
        return vpdType::IPZ_VPD;
    }
    else if (vpdVector[KW_VPD_DATA_START] == KW_VPD_START_TAG)
    {
        // KEYWORD VPD FORMAT
        return vpdType::KEYWORD_VPD;
    }
    else if (is11SFormat.compare(MEMORY_VPD_START_TAG) == 0)
    {
        // Memory VPD format
        return vpdType::MEMORY_VPD;
    }

    // INVALID VPD FORMAT
    return vpdType::INVALID_VPD_FORMAT;
}

const string getIM(const Parsed& vpdMap)
{
    Binary imVal;
    auto property = vpdMap.find("VSBP");
    if (property != vpdMap.end())
    {
        auto kw = (property->second).find("IM");
        if (kw != (property->second).end())
        {
            copy(kw->second.begin(), kw->second.end(), back_inserter(imVal));
        }
    }

    ostringstream oss;
    for (auto& i : imVal)
    {
        oss << setw(2) << setfill('0') << hex << static_cast<int>(i);
    }

    return oss.str();
}

const string getHW(const Parsed& vpdMap)
{
    Binary hwVal;
    auto prop = vpdMap.find("VINI");
    if (prop != vpdMap.end())
    {
        auto kw = (prop->second).find("HW");
        if (kw != (prop->second).end())
        {
            copy(kw->second.begin(), kw->second.end(), back_inserter(hwVal));
        }
    }

    ostringstream hwString;
    for (auto& i : hwVal)
    {
        hwString << setw(2) << setfill('0') << hex << static_cast<int>(i);
    }

    return hwString.str();
}

string getSystemsJson(const Parsed& vpdMap)
{
    string jsonPath = "/usr/share/vpd/";
    string jsonName{};

    ifstream systemJson(SYSTEM_JSON);
    if (!systemJson)
    {
        throw((VpdJsonException("Failed to access Json path", SYSTEM_JSON)));
    }

    try
    {
        auto js = json::parse(systemJson);

        const string hwKeyword = getHW(vpdMap);
        const string imKeyword = getIM(vpdMap);

        if (js.find("system") == js.end())
        {
            throw runtime_error("Invalid systems Json");
        }

        if (js["system"].find(imKeyword) == js["system"].end())
        {
            throw runtime_error(
                "Invalid system. This system type is not present "
                "in the systemsJson. IM: " +
                imKeyword);
        }

        if ((js["system"][imKeyword].find("constraint") !=
             js["system"][imKeyword].end()) &&
            (hwKeyword == js["system"][imKeyword]["constraint"]["HW"]))
        {
            jsonName = js["system"][imKeyword]["constraint"]["json"];
        }
        else if (js["system"][imKeyword].find("default") !=
                 js["system"][imKeyword].end())
        {
            jsonName = js["system"][imKeyword]["default"];
        }
        else
        {
            throw runtime_error(
                "Bad System json. Neither constraint nor default found");
        }

        jsonPath += jsonName;
    }

    catch (json::parse_error& ex)
    {
        throw(VpdJsonException("Json Parsing failed", SYSTEM_JSON));
    }
    return jsonPath;
}

void udevToGenericPath(string& file)
{
    // Sample udevEvent i2c path :
    // "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a480.i2c-bus/i2c-8/8-0051/8-00510/nvmem"
    // find if the path contains the word i2c in it.
    if (file.find("i2c") != string::npos)
    {
        string i2cBusAddr{};

        // Every udev i2c path should have the common pattern
        // "i2c-bus_number/bus_number-vpd_address". Search for
        // "bus_number-vpd_address".
        regex i2cPattern("((i2c)-[0-9]+\\/)([0-9]+-[0-9]{4})");
        smatch match;
        if (regex_search(file, match, i2cPattern))
        {
            i2cBusAddr = match.str(3);
        }
        else
        {
            cerr << "The given udev path < " << file
                 << " > doesn't match the required pattern. Skipping VPD "
                    "collection."
                 << endl;
            exit(EXIT_SUCCESS);
        }
        // Forming the generic file path
        file = i2cPathPrefix + i2cBusAddr + "/eeprom";
    }
    // Sample udevEvent spi path :
    // "/sys/devices/platform/ahb/ahb:apb/1e79b000.fsi/fsi-master/fsi0/slave@00:00/00:00:00:04/spi_master/spi2/spi2.0/spi2.00/nvmem"
    // find if the path contains the word spi in it.
    else if (file.find("spi") != string::npos)
    {
        // Every udev spi path will have common pattern "spi<Digit>/", which
        // describes the spi bus number at which the fru is connected; Followed
        // by a slash following the vpd address of the fru. Taking the above
        // input as a common key, we try to search for the pattern "spi<Digit>/"
        // using regular expression.
        regex spiPattern("((spi)[0-9]+)(\\/)");
        string spiBus{};
        smatch match;
        if (regex_search(file, match, spiPattern))
        {
            spiBus = match.str(1);
        }
        else
        {
            cerr << "The given udev path < " << file
                 << " > doesn't match the required pattern. Skipping VPD "
                    "collection."
                 << endl;
            exit(EXIT_SUCCESS);
        }
        // Forming the generic path
        file = spiPathPrefix + spiBus + ".0/eeprom";
    }
    else
    {
        cerr << "\n The given EEPROM path < " << file
             << " > is not valid. It's neither I2C nor "
                "SPI path. Skipping VPD collection.."
             << endl;
        exit(EXIT_SUCCESS);
    }
}
string getBadVpdName(const string& file)
{
    string badVpd = BAD_VPD_DIR;
    if (file.find("i2c") != string::npos)
    {
        badVpd += "i2c-";
        regex i2cPattern("(at24/)([0-9]+-[0-9]+)\\/");
        smatch match;
        if (regex_search(file, match, i2cPattern))
        {
            badVpd += match.str(2);
        }
    }
    else if (file.find("spi") != string::npos)
    {
        regex spiPattern("((spi)[0-9]+)(.0)");
        smatch match;
        if (regex_search(file, match, spiPattern))
        {
            badVpd += match.str(1);
        }
    }
    return badVpd;
}

void dumpBadVpd(const string& file, const Binary& vpdVector)
{
    fs::path badVpdDir = BAD_VPD_DIR;
    fs::create_directory(badVpdDir);
    string badVpdPath = getBadVpdName(file);
    if (fs::exists(badVpdPath))
    {
        std::error_code ec;
        fs::remove(badVpdPath, ec);
        if (ec) // error code
        {
            string error = "Error removing the existing broken vpd in ";
            error += badVpdPath;
            error += ". Error code : ";
            error += ec.value();
            error += ". Error message : ";
            error += ec.message();
            throw runtime_error(error);
        }
    }
    ofstream badVpdFileStream(badVpdPath, ofstream::binary);
    if (!badVpdFileStream)
    {
        throw runtime_error("Failed to open bad vpd file path in /tmp/bad-vpd. "
                            "Unable to dump the broken/bad vpd file.");
    }
    badVpdFileStream.write(reinterpret_cast<const char*>(vpdVector.data()),
                           vpdVector.size());
}

const string getKwVal(const Parsed& vpdMap, const string& rec,
                      const string& kwd)
{
    string kwVal{};

    auto findRec = vpdMap.find(rec);

    // check if record is found in map we got by parser
    if (findRec != vpdMap.end())
    {
        auto findKwd = findRec->second.find(kwd);

        if (findKwd != findRec->second.end())
        {
            kwVal = findKwd->second;
        }
    }

    return kwVal;
}

string createDriverCmnd(const string& devAddr, const string& command)
{
    string i2cBus, i2cReg;
    vector<string> result;
    boost::split(result, devAddr, boost::is_any_of("-"));
    if (result.size())
    {
        i2cBus = result[0];
        i2cReg = result[1];

        // remove 0s from begining
        const regex pattern("^0+(?!$)");
        i2cReg = regex_replace(i2cReg, pattern, "");
    }
    else
    {
        log<level::ERR>(
            "Wrong format of device address in Json",
            entry("ERROR=%s",
                  ("device-driver command can't be created for - " + devAddr)
                      .c_str()));

        exit(-1);
    }

    if (command == "bind" || command == "unbind")
    {
        return ("echo " + devAddr + " > /sys/bus/i2c/drivers/at24/" + command);
    }
    else if (command == "new_device")
    {
        return ("echo 24c32 0x" + i2cReg + " > /sys/bus/i2c/devices/i2c-" +
                i2cBus + "/" + command);
    }
    else
    {
        return ("echo 0x" + i2cReg + " > /sys/bus/i2c/devices/i2c-" + i2cBus +
                "/" + command);
    }
}

} // namespace vpd
} // namespace openpower