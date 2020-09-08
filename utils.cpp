#include "config.h"

#include "utils.hpp"

#include "defines.hpp"
#include "vpd_exceptions.hpp"

#include <fstream>
#include <iomanip>
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
namespace inventory
{

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface)
{
    auto mapper = bus.new_method_call(mapperDestination, mapperObjectPath,
                                      mapperInterface, "GetObject");
    mapper.append(path, std::vector<std::string>({interface}));

    std::map<std::string, std::vector<std::string>> response;
    try
    {
        auto reply = bus.call(mapper);
        reply.read(response);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("D-Bus call exception",
                        entry("OBJPATH=%s", mapperObjectPath),
                        entry("INTERFACE=%s", mapperInterface),
                        entry("EXCEPTION=%s", e.what()));

        throw std::runtime_error("Service name is not found");
    }

    if (response.empty())
    {
        throw std::runtime_error("Service name response is empty");
    }

    return response.begin()->first;
}

void callPIM(ObjectMap&& objects)
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto service = getService(bus, pimPath, pimIntf);
        auto pimMsg =
            bus.new_method_call(service.c_str(), pimPath, pimIntf, "Notify");
        pimMsg.append(std::move(objects));
        auto result = bus.call(pimMsg);
        if (result.is_method_error())
        {
            std::cerr << "PIM Notify() failed\n";
        }
    }
    catch (const std::runtime_error& e)
    {
        log<level::ERR>(e.what());
    }
}

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
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Error in mapper GetSubTree",
                        entry("ERROR=%s", e.what()));
    }

    return result;
}

} // namespace inventory

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
               const std::string& errIntf)
{
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto service = getService(bus, loggerObjectPath, loggerCreateInterface);
        auto method = bus.new_method_call(service.c_str(), loggerObjectPath,
                                          loggerCreateInterface, "Create");

        method.append(errIntf, "xyz.openbmc_project.Logging.Entry.Level.Error",
                      additionalData);
        auto resp = bus.call(method);
    }
    catch (const sdbusplus::exception::SdBusError& e)
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

string udevToGenericPath(const string& udevPath)
{
    string file{};

    // Sample udevEvent i2c path :
    // "/sys/devices/platform/ahb/ahb:apb/ahb:apb:bus@1e78a000/1e78a480.i2c-bus/i2c-8/8-0051/8-00510/nvmem"
    // find if the path contains the word i2c in it.
    if (udevPath.find("i2c") != string::npos)
    {
        string i2cBus{};

        // Every udev i2c path will have common pattern "i2c-digit/", which
        // describes the i2c bus number at which the fru is connected; followed
        // by a slash following the vpd address of the fru. Taking the above
        // input as a common key, we try to match the pattern "i2c-digit/" using
        // regular expression.
        regex i2cPattern("i2c-[0-9]+\\/");
        auto i2cWord =
            sregex_iterator(udevPath.begin(), udevPath.end(), i2cPattern);
        for (auto i = i2cWord; i != sregex_iterator(); ++i)
        {
            smatch match = *i;
            i2cBus = match.str();
        }

        // Replacing the i2c udev path with the words succeeding the pattern
        // match; Say, "8-0051/8-00510/nvmem" - prefixing
        // "/sys/bus/i2c/drivers/at24/" - gives the complete i2c generic path.
        regex udevPattern("[^\\s]+" + i2cBus);
        file = std::regex_replace(udevPath, udevPattern, i2cPathPrefix);
    }

    // Sample udevEvent spi path :
    // "/sys/devices/platform/ahb/ahb:apb/1e79b000.fsi/fsi-master/fsi0/slave@00:00/00:00:00:04/spi_master/spi2/spi2.0/spi2.00/nvmem"
    // find if the path contains the word spi in it.
    else if (udevPath.find("spi") != string::npos)
    {

        // Every udev spi path will have common pattern "spi<Digit>/", which
        // describes the spi bus number at which the fru is connected; Followed
        // by a slash following the vpd address of the fru. Taking the above
        // input as a common key, we try to match the pattern "spi<Digit>/"
        // using regular expression.
        regex spiPattern("((spi)[0-9]+\\/)");
        auto spiWord =
            sregex_iterator(udevPath.begin(), udevPath.end(), spiPattern);
        string spiBus{};
        for (auto i = spiWord; i != sregex_iterator(); ++i)
        {
            smatch match = *i;
            spiBus = match.str();
        }

        // Replacing the spi udev path with the words succeeding the pattern
        // match; Say, "spi2.0/spi2.00/nvmem" - prefixing
        // "/sys/bus/spi/drivers/at25/" - gives the complete spi generic path
        regex udevPattern("[^\\s]+" + spiBus);
        file = std::regex_replace(udevPath, udevPattern, spiPathPrefix);
    }
    return file;
}
} // namespace vpd
} // namespace openpower
