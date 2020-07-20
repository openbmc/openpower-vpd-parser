#include "config.h"

#include "const.hpp"
#include "defines.hpp"
#include "ipz_parser.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "parser_factory.hpp"
#include "utils.hpp"

#include <ctype.h>

#include <CLI/CLI.hpp>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>

using namespace std;
using namespace openpower::vpd;
using namespace CLI;
using namespace vpd::keyword::parser;
using namespace openpower::vpd::constants;
namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace openpower::vpd::parser::factory;
using namespace openpower::vpd::inventory;
using namespace openpower::vpd::memory::parser;
using namespace openpower::vpd::parser::interface;

/** @brief An api to get keywords which needs to be published on DBus
 *  @param[in] - map of record, keyword and keyword data
 *  @return - A map which contains keyword and data that needs to be published
 *  on interface for this record.
 */
inventory::DbusPropertyMap
    getDBusPropertyMap(inventory::DbusPropertyMap dbusProperties,
                       const vector<inventory::Keyword>& kwdsToPublish)
{
    for (auto it = dbusProperties.begin(); it != dbusProperties.end();)
    {
        if (find(kwdsToPublish.begin(), kwdsToPublish.end(), it->first) ==
            kwdsToPublish.end())
        {
            it = dbusProperties.erase(it);
        }
        else
        {
            it++;
        }
    }
    return dbusProperties;
}

/**
 * @brief Expands location codes
 */
static auto expandLocationCode(const string& unexpanded, const Parsed& vpdMap,
                               bool isSystemVpd)
{
    auto expanded{unexpanded};
    static constexpr auto SYSTEM_OBJECT = "/system/chassis/motherboard";
    static constexpr auto VCEN_IF = "com.ibm.ipzvpd.VCEN";
    static constexpr auto VSYS_IF = "com.ibm.ipzvpd.VSYS";
    size_t idx = expanded.find("fcs");
    try
    {
        if (idx != string::npos)
        {
            string fc{};
            string se{};
            if (isSystemVpd)
            {
                const auto& fcData = vpdMap.at("VCEN").at("FC");
                const auto& seData = vpdMap.at("VCEN").at("SE");
                fc = string(fcData.data(), fcData.size());
                se = string(seData.data(), seData.size());
            }
            else
            {
                fc = readBusProperty(SYSTEM_OBJECT, VCEN_IF, "FC");
                se = readBusProperty(SYSTEM_OBJECT, VCEN_IF, "SE");
            }

            // TODO: See if ND1 can be placed in the JSON
            expanded.replace(idx, 3, fc.substr(0, 4) + ".ND1." + se);
        }
        else
        {
            idx = expanded.find("mts");
            if (idx != string::npos)
            {
                string mt{};
                string se{};
                if (isSystemVpd)
                {
                    const auto& mtData = vpdMap.at("VSYS").at("TM");
                    const auto& seData = vpdMap.at("VSYS").at("SE");
                    mt = string(mtData.data(), mtData.size());
                    se = string(seData.data(), seData.size());
                }
                else
                {
                    mt = readBusProperty(SYSTEM_OBJECT, VSYS_IF, "TM");
                    se = readBusProperty(SYSTEM_OBJECT, VSYS_IF, "SE");
                }

                replace(mt.begin(), mt.end(), '-', '.');
                expanded.replace(idx, 3, mt + "." + se);
            }
        }
    }
    catch (exception& e)
    {
        cerr << "Failed to expand location code with exception: " << e.what()
             << "\n";
    }
    return expanded;
}
/**
 * @brief Populate FRU specific interfaces.
 *
 * This is a common method which handles both
 * ipz and keyword specific interfaces thus,
 * reducing the code redundancy.
 * @param[in] map - Reference to the innermost keyword-value map.
 * @param[in] preIntrStr - Reference to the interface string.
 * @param[out] interfaces - Reference to interface map.
 */
template <typename T>
static void populateFruSpecificInterfaces(const T& map,
                                          const string& preIntrStr,
                                          inventory::InterfaceMap& interfaces)
{
    inventory::PropertyMap prop;

    for (const auto& kwVal : map)
    {
        vector<uint8_t> vec(kwVal.second.begin(), kwVal.second.end());

        auto kw = kwVal.first;

        if (kw[0] == '#')
        {
            kw = string("PD_") + kw[1];
        }
        else if (isdigit(kw[0]))
        {
            kw = string("N_") + kw;
        }
        prop.emplace(move(kw), move(vec));
    }

    interfaces.emplace(preIntrStr, move(prop));
}

/**
 * @brief Populate Interfaces.
 *
 * This method populates common and extra interfaces to dbus.
 * @param[in] js - json object
 * @param[out] interfaces - Reference to interface map
 * @param[in] vpdMap - Reference to the parsed vpd map.
 * @param[in] isSystemVpd - Denotes whether we are collecting the system VPD.
 */
template <typename T>
static void populateInterfaces(const nlohmann::json& js,
                               inventory::InterfaceMap& interfaces,
                               const T& vpdMap, bool isSystemVpd)
{
    for (const auto& ifs : js.items())
    {
        string inf = ifs.key();
        inventory::PropertyMap props;

        for (const auto& itr : ifs.value().items())
        {
            const string& busProp = itr.key();

            if (itr.value().is_boolean())
            {
                props.emplace(busProp, itr.value().get<bool>());
            }
            else if (itr.value().is_string())
            {
                if constexpr (is_same<T, Parsed>::value)
                {
                    if (busProp == "LocationCode" &&
                        inf == "com.ibm.ipzvpd.Location")
                    {
                        auto prop = expandLocationCode(
                            itr.value().get<string>(), vpdMap, isSystemVpd);
                        props.emplace(busProp, prop);
                    }
                    else
                    {
                        props.emplace(busProp, itr.value().get<string>());
                    }
                }
                else
                {
                    props.emplace(busProp, itr.value().get<string>());
                }
            }
            else if (itr.value().is_object())
            {
                const string& rec = itr.value().value("recordName", "");
                const string& kw = itr.value().value("keywordName", "");
                const string& encoding = itr.value().value("encoding", "");

                if constexpr (is_same<T, Parsed>::value)
                {
                    if (!rec.empty() && !kw.empty() && vpdMap.count(rec) &&
                        vpdMap.at(rec).count(kw))
                    {
                        auto encoded =
                            encodeKeyword(vpdMap.at(rec).at(kw), encoding);
                        props.emplace(busProp, encoded);
                    }
                }
                else if constexpr (is_same<T, KeywordVpdMap>::value)
                {
                    if (!kw.empty() && vpdMap.count(kw))
                    {
                        auto prop =
                            string(vpdMap.at(kw).begin(), vpdMap.at(kw).end());
                        auto encoded = encodeKeyword(prop, encoding);
                        props.emplace(busProp, encoded);
                    }
                }
            }
        }
        interfaces.emplace(inf, move(props));
    }
}

Binary getVpdDataInVector(nlohmann::json& js, const string& filePath)
{
    uint32_t offset = 0;
    // check if offset present?
    for (const auto& item : js["frus"][filePath])
    {
        if (item.find("offset") != item.end())
        {
            offset = item["offset"];
        }
    }
    char buf[2048];
    ifstream vpdFile;
    vpdFile.rdbuf()->pubsetbuf(buf, sizeof(buf));
    vpdFile.open(filePath, ios::binary);
    vpdFile.seekg(offset, ios_base::cur);

    // Read 64KB data content of the binary file into a vector
    Binary tmpVector((istreambuf_iterator<char>(vpdFile)),
                     istreambuf_iterator<char>());

    vector<unsigned char>::const_iterator first = tmpVector.begin();
    vector<unsigned char>::const_iterator last = tmpVector.begin() + NEXT_64_KB;

    if (distance(first, last) < static_cast<int>(tmpVector.size()))
    {
        Binary vpdVector(first, last);
        return vpdVector;
    }

    return tmpVector;
}

/**
 * @brief Prime the Inventory
 * Prime the inventory by populating only the location code,
 * type interface and the inventory object for the frus
 * which are not system vpd fru.
 *
 * @param[in] jsObject - Reference to vpd inventory json object
 * @param[in] vpdMap -  Reference to the parsed vpd map
 */
template <typename T>
inventory::ObjectMap primeInventory(nlohmann::json& jsObject, const T& vpdMap)
{
    inventory::ObjectMap objects;
    if (jsObject.find("frus") == jsObject.end())
    {
        throw runtime_error("Frus missing in Inventory json");
    }

    for (auto itemFRUS : jsObject["frus"].items())
    {
        for (auto itemEEPROM : itemFRUS.value())
        {
            inventory::InterfaceMap interfaces;
            auto isSystemVpd = itemEEPROM.value("isSystemVpd", false);
            sdbusplus::message::object_path object(
                itemEEPROM.at("inventoryPath"));

            if (!isSystemVpd)
            {
                for (const auto& eI : itemEEPROM["extraInterfaces"].items())
                {
                    inventory::PropertyMap props;
                    if (eI.key() == "com.ibm.ipzvpd.Location")
                    {
                        if constexpr (is_same<T, Parsed>::value)
                        {
                            for (auto& lC : eI.value().items())
                            {
                                auto propVal =
                                    expandLocationCode(lC.value().get<string>(),
                                                       vpdMap, isSystemVpd);

                                props.emplace(move(lC.key()), move(propVal));
                                interfaces.emplace(move(eI.key()), move(props));
                            }
                        }
                    }
                    else if (eI.key().find("Inventory.Item.") != string::npos)
                    {
                        interfaces.emplace(move(eI.key()), move(props));
                    }
                }
                objects.emplace(move(object), move(interfaces));
            }
        }
    }
    return objects;
}

/**
 * @brief Populate Dbus.
 * This method invokes all the populateInterface functions
 * and notifies PIM about dbus object.
 * @param[in] vpdMap - Either IPZ vpd map or Keyword vpd map based on the
 * input.
 * @param[in] js - Inventory json object
 * @param[in] filePath - Path of the vpd file
 * @param[in] preIntrStr - Interface string
 */
template <typename T>
static void populateDbus(const T& vpdMap, nlohmann::json& js,
                         const string& filePath)
{
    inventory::InterfaceMap interfaces;
    inventory::ObjectMap objects;
    inventory::PropertyMap prop;

    ifstream propertyJson("dbus_properties.json");
    json dbusProperty;
    bool publishSlectedKeywords = false;
    // skip the implementation of selected keyword publish on Dbus in case this
    // json is not present.
    if (propertyJson.is_open())
    {
        publishSlectedKeywords = true;
        auto dbusPropertyJson = json::parse(propertyJson);
        if (dbusPropertyJson.find("dbusProperties") == dbusPropertyJson.end())
        {
            throw runtime_error("Dbus property json error");
        }

        dbusProperty = dbusPropertyJson["dbusProperties"];
    }

    bool isSystemVpd = false;
    for (const auto& item : js["frus"][filePath])
    {
        const auto& objectPath = item["inventoryPath"];
        sdbusplus::message::object_path object(objectPath);
        isSystemVpd = item.value("isSystemVpd", false);
        // Populate the VPD keywords and the common interfaces only if we
        // are asked to inherit that data from the VPD, else only add the
        // extraInterfaces.
        if (item.value("inherit", true))
        {
            if constexpr (is_same<T, Parsed>::value)
            {
                // Each record in the VPD becomes an interface and all
                // keyword within the record are properties under that
                // interface.
                for (const auto& record : vpdMap)
                {
                    if (publishSlectedKeywords)
                    {
                        if (dbusProperty.contains(record.first))
                        {
                            const vector<inventory::Keyword>& kwdsToPublish =
                                dbusProperty[record.first];
                            inventory::DbusPropertyMap busPropertyMap =
                                getDBusPropertyMap(record.second,
                                                   kwdsToPublish);
                            populateFruSpecificInterfaces(
                                busPropertyMap, ipzVpdInf + record.first,
                                interfaces);
                        }
                    }
                    else
                    {
                        populateFruSpecificInterfaces(record.second,
                                                      ipzVpdInf + record.first,
                                                      interfaces);
                    }
                }
            }
            else if constexpr (is_same<T, KeywordVpdMap>::value)
            {
                populateFruSpecificInterfaces(vpdMap, kwdVpdInf, interfaces);
            }
        }
        else
        {
            // Check if we have been asked to inherit specific record(s)
            if constexpr (is_same<T, Parsed>::value)
            {
                if (item.find("copyRecords") != item.end())
                {
                    for (const auto& record : item["copyRecords"])
                    {
                        const string& recordName = record;
                        if (vpdMap.find(recordName) != vpdMap.end())
                        {
                            populateFruSpecificInterfaces(
                                vpdMap.at(recordName), ipzVpdInf + recordName,
                                interfaces);
                        }
                    }
                }
            }
        }

        /*        if (item.value("inheritEI", true))
                {
                    // Populate interfaces and properties that are common to
           every FRU
                    // and additional interface that might be defined on a
           per-FRU
                    // basis.
                    if (item.find("extraInterfaces") != item.end())
                    {
                        populateInterfaces(item["extraInterfaces"], interfaces,
           vpdMap, isSystemVpd);
                    }
                }
        */
        // this condition is needed as openpower json will not have location
        // code interface
        if (item["extraInterfaces"].find(LOCATION_CODE_INF) !=
            item["extraInterfaces"].end())
        {
            /*add Common interface to all the fru except the one having "mts" in
            their location code as they will not inherit CI*/
            const string& LocationCode =
                item["extraInterfaces"][LOCATION_CODE_INF]["LocationCode"]
                    .get_ref<const nlohmann::json::string_t&>();
            if (LocationCode.substr(1, 3) != "mts")
            {
                if (js.find("commonInterfaces") != js.end())
                {
                    populateInterfaces(js["commonInterfaces"], interfaces,
                                       vpdMap, isSystemVpd);
                }
            }
        }

        objects.emplace(move(object), move(interfaces));
    }

    /*   if (isSystemVpd)
       {
           vector<uint8_t> imVal;
           if constexpr (is_same<T, Parsed>::value)
           {
               auto property = vpdMap.find("VSBP");
               if (property != vpdMap.end())
               {
                   auto value = (property->second).find("IM");
                   if (value != (property->second).end())
                   {
                       //                          imVal = value->second;
                       copy(value->second.begin(), value->second.end(),
                            back_inserter(imVal));
                   }
               }
           }

           fs::path target;
           fs::path link = "/var/lib/vpd/vpd_inventory.json";

           ostringstream oss;
           for (auto& i : imVal)
           {
               if ((int)i / 10 == 0) // one digit number
               {
                   oss << hex << 0;
               }
               oss << hex << static_cast<int>(i);
           }
           string imValStr = oss.str();

           if (imValStr == "50001000") // 4U
           {
               target = "/usr/share/vpd/50001000.json";
           }

           else if (imValStr == "50001001") // 2U
           {
               target = "/usr/share/vpd/50001001.json";
           }

           // unlink the symlink which is created at build time
           remove("/var/lib/vpd/vpd_inventory.json");
           // create a new symlink based on the system
           fs::create_symlink(target, link);

           // Reloading the json
           ifstream inventoryJson(link);
           auto js = json::parse(inventoryJson);
           inventoryJson.close();

           inventory::ObjectMap primeObject = primeInventory(js, vpdMap);
           objects.insert(primeObject.begin(), primeObject.end());
       }
   */
    // Notify PIM
    inventory::callPIM(move(objects));
}

tuple<uint16_t, uint16_t> checkSNandFNoffset(const json& js,
                                             const string& filePath)
{
    tuple<int16_t, int16_t> offset{
        -1, -1}; //{offsets::Offsets::INVALID, offsets::Offsets::INVALID};

    for (const auto& itemEEPROM : js["frus"][filePath])
    {
        // if the file has this key then look for sn and fn offset file
        if (itemEEPROM.find("offsetFilePath") != itemEEPROM.end())
        {
            std::string offsetFilePath = itemEEPROM["offsetFilePath"];

            std::ifstream offsetJSon(offsetFilePath);
            if (!offsetJSon)
            {
                // imples the file does not exits, so progress with parsing the
                // complete file again
                return offset;
            }

            auto js = json::parse(offsetJSon);

            if (js.find("snOffset") != js.end() &&
                js.find("fnOffset") != js.end())
            {
                // return offset of SN and FN kwd
                offset = make_tuple(js["snOffset"], js["fnOffset"]);
                return offset;
            }

            // if offset is not found return invalid offset.
            return offset;
        }
    }
}

tuple<string, string> getDataFromFile(tuple<uint16_t, uint16_t> offset,
                                      const Binary& vpdFile)
{
    uint16_t snOffset = get<0>(offset);
    uint16_t fnOffset = get<1>(offset);

    auto iterator = vpdFile.cbegin();
    advance(iterator, snOffset);

    string snData(iterator, iterator + SERIAL_NUM_LEN);

    iterator = vpdFile.cbegin();
    advance(iterator, fnOffset);

    string fnData(iterator, iterator + FRU_NUM_LEN);

    return make_tuple(snData, fnData);
}

tuple<string, string> getDataFromDbus(const json& js, const string& filePath)
{
    string interface = ipzVpdInf + string("VINI");
    string objPath{};
    string snData{};
    string fnData{};

    for (const auto& itemEEPROM : js["frus"][filePath])
    {
        if (itemEEPROM.value("inherit", true))
        {
            objPath = itemEEPROM["inventoryPath"];

            // we need object path of any fru that has inherit as true under
            // this file path
            break;
        }
    }

    if (!objPath.empty())
    {
        // read bus property for SN kwd
        snData = readBusProperty(objPath, interface, "SN");

        // read bus property for FN kwd
        fnData = readBusProperty(objPath, interface, "FN");
    }

    return make_tuple(snData, fnData);
}

bool compareData(tuple<string, string> fileData, tuple<string, string> busData)
{
    if ((get<0>(fileData) == get<0>(busData)) &&
        (get<1>(fileData) == get<1>(busData)))
    {
        return true;
    }
    return false;
}

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        App app{"ibm-read-vpd - App to read VPD, parse it and store "
                "in DBUS"};
        string file{};
        bool doDump = false;

        app.add_flag("--dump", doDump, "Optional argument to dump vpd");

        app.add_option("-f, --file", file, "File containing VPD")
            ->required()
            ->check(ExistingFile);

        CLI11_PARSE(app, argc, argv);

        // Make sure that the file path we get is for a supported EEPROM
        ifstream inventoryJson("vpd_inventory.json");
        auto js = json::parse(inventoryJson);

        if ((js.find("frus") == js.end()) ||
            (js["frus"].find(file) == js["frus"].end()))
        {
            cout << "Device path not in JSON, ignoring" << endl;
            return 0;
        }

        uint32_t offset = 0;
        // check if offset present?
        for (const auto& item : js["frus"][file])
        {
            if (item.find("offset") != item.end())
            {
                offset = item["offset"];
            }
        }

        char buf[2048];
        ifstream vpdFile;
        vpdFile.rdbuf()->pubsetbuf(buf, sizeof(buf));
        vpdFile.open(file, ios::binary);
        vpdFile.seekg(offset, std::ios_base::cur);

        // Read 64KB data content of the binary file into a vector
        Binary tmpVector((istreambuf_iterator<char>(vpdFile)),
                         istreambuf_iterator<char>());

        vector<unsigned char>::const_iterator first = tmpVector.begin();
        vector<unsigned char>::const_iterator last = tmpVector.begin() + 65536;

        Binary vpdVector(first, last);

        // check if vpd file is empty
        if (vpdVector.empty())
        {
            throw runtime_error(
                "VPD file is empty. Can't process with blank file.");
        }

        tuple<int16_t, int16_t> kwdOffsets = checkSNandFNoffset(js, file);
        if (get<0>(kwdOffsets) != INVALID && get<1>(kwdOffsets) != INVALID)
        {
            tuple<string, string> fileData =
                getDataFromFile(kwdOffsets, vpdVector);

            tuple<string, string> busData = getDataFromDbus(js, file);

            if (compareData(fileData, busData))
            {
                std::cout << "retrn from compare data" << std::endl;
                return rc;
            }
        }

        ParserInterface* parser =
            ParserFactory::getParser(std::move(vpdVector), file);

        variant<KeywordVpdMap, Store> parseResult;
        parseResult = parser->parse();

        if (auto pVal = get_if<Store>(&parseResult))
        {
            if (doDump)
            {
                pVal->dump();
                return 0;
            }
            populateDbus(pVal->getVpdMap(), js, file);
        }
        else if (auto pVal = get_if<KeywordVpdMap>(&parseResult))
        {
            populateDbus(*pVal, js, file);
        }

        // release the parser object
        ParserFactory::freeParser(parser);
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
        rc = -1;
    }
    return rc;
}
