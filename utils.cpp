#include "config.h"

#include "utils.hpp"

#include "defines.hpp"

#include <openssl/sha.h>

#include <fstream>
#include <iomanip>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/server.hpp>
#include <sstream>

namespace openpower
{
namespace vpd
{
using namespace openpower::vpd::constants;
using namespace inventory;
using namespace phosphor::logging;
using namespace filestream;

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
    std::string service;

    try
    {
        auto bus = sdbusplus::bus::new_default();
        service = getService(bus, pimPath, pimIntf);
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

inventory::VPDfilepath getVpdFilePath(const json& jsonFile,
                                      const std::string& ObjPath)
{
    inventory::VPDfilepath filePath{};

    if (jsonFile.find("frus") == jsonFile.end())
    {
        throw std::runtime_error("Invalid Json");
    }

    const nlohmann::json& groupFRUS =
        jsonFile["frus"].get_ref<const nlohmann::json::object_t&>();
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

std::string getSHA(const std::string& filePath)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, filePath.c_str(), filePath.length());
    SHA256_Final(digest, &ctx);
    char mdString[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        snprintf(&mdString[i * 2], 3, "%02x", (unsigned int)digest[i]);
    }

    return std::string(mdString);
}
namespace filestream
{
std::string streamStatus(std::fstream& vpdFileStream)
{
    string status = "";
    if (vpdFileStream.eof())
    {
        status = "End of file";
    }
    else if (vpdFileStream.good())
    {
        status = "Good";
    }
    else if (vpdFileStream.fail())
    {
        status = "Fail";
    }
    else if (vpdFileStream.bad())
    {
        status = "Bad";
    }
    return status;
}
} // namespace filestream

bool eepromPresenceInJson(const std::string& eepromPath)
{
    bool present = false;
    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    auto js = json::parse(inventoryJson);

    if (js.find("frus") == js.end())
    {
        cout << "Invalid JSON structure - frus{} object not found in "
             << INVENTORY_JSON_SYM_LINK << endl;
        return 0;
    }
    json fruJson = js["frus"];
    if (fruJson.find(eepromPath) != fruJson.end())
    {
        present = true;
    }
    return present;
}

bool recKwPresenceInDbusProp(const std::string& recordName,
                             const std::string& keyword)
{
    ifstream propertyJson("dbus_properties.json");
    json dbusProperty;
    bool present = false;

    if (propertyJson.is_open())
    {
        auto dbusPropertyJson = json::parse(propertyJson);
        if (dbusPropertyJson.find("dbusProperties") == dbusPropertyJson.end())
        {
            throw runtime_error("Dbus property json error");
        }

        dbusProperty = dbusPropertyJson["dbusProperties"];
        if (dbusProperty.contains(recordName))
        {
            const vector<inventory::Keyword>& kwdsToPublish =
                dbusProperty[recordName];
            if (find(kwdsToPublish.begin(), kwdsToPublish.end(), keyword) !=
                kwdsToPublish.end()) // present
            {
                present = true;
            }
        }
    }
    else
    {
        throw runtime_error("Unable to open dbus_properties.json");
    }
    return present;
}

Binary toBinary(const std::string& value)
{
    Binary val;
    if (value.find("0x") == string::npos)
    {
        val.assign(value.begin(), value.end());
    }
    else if (value.find("0x") != string::npos)
    {
        stringstream ss;
        ss.str(value.substr(2));
        string byteStr{};

        while (!ss.eof())
        {
            ss >> setw(2) >> byteStr;
            uint8_t byte = strtoul(byteStr.c_str(), nullptr, 16);

            val.push_back(byte);
        }
    }

    else
    {
        throw runtime_error("The value to be updated should be either in ascii "
                            "or in hex. Refer --help option");
    }
    return val;
}
} // namespace vpd
} // namespace openpower
