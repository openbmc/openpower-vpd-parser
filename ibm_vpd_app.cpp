#include "config.h"

#include "defines.hpp"
#include "ibm_vpd_type_check.hpp"
#include "keyword_vpd_parser.hpp"
#include "memory_vpd_parser.hpp"
#include "parser.hpp"
#include "utils.hpp"

#include <ctype.h>

#include <CLI/CLI.hpp>
#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>

using namespace std;
using namespace openpower::vpd;
using namespace CLI;
using namespace vpd::keyword::parser;
using namespace vpdFormat;
using namespace vpd::memory::parser;
using namespace openpower::vpd::constants;

/** @brief Reads a property from the inventory manager given object path,
 * intreface and property.
 */
static auto readBusProperty(const string& obj, const string& inf,
                            const string& prop)
{
    string propVal{};
    static constexpr auto OBJ_PREFIX = "/xyz/openbmc_project/inventory";
    string object = OBJ_PREFIX + obj;
    auto bus = sdbusplus::bus::new_default();
    auto properties = bus.new_method_call(
        "xyz.openbmc_project.Inventory.Manager", object.c_str(),
        "org.freedesktop.DBus.Properties", "Get");
    properties.append(inf);
    properties.append(prop);
    auto result = bus.call(properties);
    if (!result.is_method_error())
    {
        variant<Binary> val;
        result.read(val);

        if (auto pVal = get_if<Binary>(&val))
        {
            propVal.assign(reinterpret_cast<const char*>(pVal->data()),
                           pVal->size());
        }
    }
    return propVal;
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
    catch (std::exception& e)
    {
        std::cerr << "Failed to expand location code with exception: "
                  << e.what() << "\n";
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
        std::vector<uint8_t> vec(kwVal.second.begin(), kwVal.second.end());

        auto kw = kwVal.first;

        if (kw[0] == '#')
        {
            kw = std::string("PD_") + kw[1];
        }
        else if (isdigit(kw[0]))
        {
            kw = std::string("N_") + kw;
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
                if constexpr (std::is_same<T, Parsed>::value)
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

                if constexpr (std::is_same<T, Parsed>::value)
                {
                    if (!rec.empty() && !kw.empty() && vpdMap.count(rec) &&
                        vpdMap.at(rec).count(kw))
                    {
                        auto encoded =
                            encodeKeyword(vpdMap.at(rec).at(kw), encoding);
                        props.emplace(busProp, encoded);
                    }
                }
                else if constexpr (std::is_same<T, KeywordVpdMap>::value)
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
    vpdFile.seekg(offset, std::ios_base::cur);

    // Read 64KB data content of the binary file into a vector
    Binary tmpVector((istreambuf_iterator<char>(vpdFile)),
                     istreambuf_iterator<char>());

    vector<unsigned char>::const_iterator first = tmpVector.begin();
    vector<unsigned char>::const_iterator last = tmpVector.begin() + 65536;

    Binary vpdVector(first, last);
    return vpdVector;
}

internal::KeywordMap readKeywords(Binary::const_iterator iterator)
{
    internal::KeywordMap map{};
    while (true)
    {
        // Note keyword name
        std::string kw(iterator, iterator + lengths::KW_NAME);
        if (LAST_KW == kw)
        {
            cout << "Reched to Last KW, breaking... \n";
            // We're done
            break;
        }
        // Check if the Keyword is '#kw'
        char kwNameStart = *iterator;
        // Jump past keyword name
        std::advance(iterator, lengths::KW_NAME);
        std::size_t length;
        std::size_t lengthHighByte;
        if (POUND_KW == kwNameStart)
        {
            // Note keyword data length
            length = *iterator;
            lengthHighByte = *(iterator + 1);
            length |= (lengthHighByte << 8);
            // Jump past 2Byte keyword length
            std::advance(iterator, sizeof(PoundKwSize));
        }
        else
        {
            // Note keyword data length
            length = *iterator;
            // Jump past keyword length
            std::advance(iterator, sizeof(KwSize));
        }
        auto stop = std::next(iterator, length);
        std::string kwdata(iterator, stop);
        map.emplace(std::move(kw), std::move(kwdata));

        // Jump past keyword data length
        std::advance(iterator, length);
    }

    return map;
}

Parsed processParentFruVpd(nlohmann::json& js, const string& parentFruVpdPath)
{
    Parsed vpdMap;

    // Temp hardcoded list.TODO:should get it dynamically?
    vector<string> commonIntRecordsList = {"VINI", "VR10"};

    Binary vpdVector(getVpdDataInVector(js, parentFruVpdPath));

    auto iterator = vpdVector.cbegin();
    // point to VTOC offset
    advance(iterator, 35);
    uint16_t vtocOffsetLowByte = *iterator;
    uint16_t vtocOffsetHighByte = *(iterator + 1);
    vtocOffsetLowByte |= (vtocOffsetHighByte << 8);

    iterator = vpdVector.cbegin();
    advance(iterator, vtocOffsetLowByte + 12);
    uint8_t ptLen = *iterator;

    advance(iterator, 1); // point to next record

    auto end = iterator;
    std::advance(end, ptLen);
    vector<uint32_t> offsets;

    while (iterator < end)
    {
        string recordName(iterator, iterator + 4); // Read Record name

        if (find(commonIntRecordsList.begin(), commonIntRecordsList.end(),
                 recordName) != commonIntRecordsList.end())
        {
            // collect it's offset
            uint16_t offset;
            advance(iterator, 4 + 2);

            uint16_t recOffsetLowByte = *iterator;
            uint16_t recOffsetHighByte = *(iterator + 1);
            offset = recOffsetLowByte | (recOffsetHighByte << 8);

            offsets.push_back(offset);

            // move to next record
            advance(iterator, 2 + 2 + 2 + 2);
        }
        else
        {
            // move to next record
            advance(iterator, 4 + 2 + 2 + 2 + 2 + 2);
        }
    } // Got Record's offset list

    for (const auto& offset : offsets)
    {
        iterator = vpdVector.cbegin();
        // get records and kw-data and store it in parsed type.
        advance(iterator, offset + sizeof(RecordId) + sizeof(RecordSize) +
                              lengths::KW_NAME + sizeof(KwSize));

        string name(iterator, iterator + lengths::RECORD_NAME);
        advance(iterator, lengths::RECORD_NAME);

        auto kwMap = readKeywords(iterator);
        vpdMap.emplace(std::move(name), std::move(kwMap));
    }
    return vpdMap;
}

Parsed getFruCiVpdMap(nlohmann::json& js, const string& moduleObjPath)
{
    string parentFruVpdPath;
    bool parentFruFound = false;

    // get all FRUs list
    for (const auto& eachFru : js["frus"].items())
    {
        bool moduleObjPathMatched = false;
        bool parentFru = false;
        for (const auto& eachInventory : eachFru.value())
        {
            const auto& thisObjectPath = eachInventory["inventoryPath"];

            // "type" exists only in CPU module and FRU
            if (eachInventory.find("type") != eachInventory.end())
            {
                // If inventory type is fruAndModule then set flag
                if (eachInventory["type"] == "fruAndModule")
                {
                    parentFru = true;
                }
            }

            if (thisObjectPath == moduleObjPath)
            {
                moduleObjPathMatched = true;
            }
        }

        // If condition satisfies then collect this sys path and exit
        if (parentFru && moduleObjPathMatched)
        {
            parentFruVpdPath = eachFru.key();
            break;
        }
    }

    // TODO 1:Handle if parentFruFound NOT found from JSON
    // TODO 2:Handle if parentFruFound NOT present on the System

    // process this parent vpd to get CI
    const auto& commonIntrfVpdMap = processParentFruVpd(js, parentFruVpdPath);

    return commonIntrfVpdMap;
}

/**
 * @brief Populate Dbus.
 *
 * This method invokes all the populateInterface functions
 * and notifies PIM about dbus object.
 * @param[in] vpdMap - Either IPZ vpd map or Keyword vpd map based on the input.
 * @param[in] js - Inventory json object
 * @param[in] filePath - Path of the vpd file
 * @param[in] preIntrStr - Interface string
 */
template <typename T>
static void populateDbus(const T& vpdMap, nlohmann::json& js,
                         const string& filePath, const string& preIntrStr)
{
    inventory::InterfaceMap interfaces;
    inventory::ObjectMap objects;
    inventory::PropertyMap prop;

    for (const auto& item : js["frus"][filePath])
    {
        const auto& objectPath = item["inventoryPath"];
        sdbusplus::message::object_path object(objectPath);
        auto isSystemVpd = item.value("isSystemVpd", false);
        // Populate the VPD keywords and the common interfaces only if we
        // are asked to inherit that data from the VPD, else only add the
        // extraInterfaces.
        if (item.value("inherit", true))
        {
            if constexpr (std::is_same<T, Parsed>::value)
            {
                // Each record in the VPD becomes an interface and all keyword
                // within the record are properties under that interface.
                for (const auto& record : vpdMap)
                {
                    populateFruSpecificInterfaces(
                        record.second, preIntrStr + record.first, interfaces);
                }
            }
            else if constexpr (std::is_same<T, KeywordVpdMap>::value)
            {
                populateFruSpecificInterfaces(vpdMap, preIntrStr, interfaces);
            }
            if (js.find("commonInterfaces") != js.end())
            {
                populateInterfaces(js["commonInterfaces"], interfaces, vpdMap,
                                   isSystemVpd);
            }
        }
        else
        {
            // Check if we have been asked to inherit specific record(s)
            if constexpr (std::is_same<T, Parsed>::value)
            {
                if (item.find("copyRecords") != item.end())
                {
                    for (const auto& record : item["copyRecords"])
                    {
                        const string& recordName = record;
                        if (vpdMap.find(recordName) != vpdMap.end())
                        {
                            populateFruSpecificInterfaces(
                                vpdMap.at(recordName), preIntrStr + recordName,
                                interfaces);
                        }
                    }
                }
                // check if it is a module Type, then find it's Parent FRU to
                // get CI.
                if (item.find("type") != item.end())
                {
                    if (item["type"] == "moduleOnly")
                    {
                        const auto& moduleObjPath = item["inventoryPath"];
                        auto moduleCiVpdMap = getFruCiVpdMap(js, moduleObjPath);
                        // Use this parsed type moduleCIvpdMap.
                        if constexpr (std::is_same<T, Parsed>::value)
                        {
                            for (const auto& record : moduleCiVpdMap)
                            {
                                populateFruSpecificInterfaces(
                                    record.second, preIntrStr + record.first,
                                    interfaces);
                            }
                        }
                        populateInterfaces(js["commonInterfaces"], interfaces,
                                           moduleCiVpdMap, isSystemVpd);
                    }
                }
            }
        }

        // Populate interfaces and properties that are common to every FRU
        // and additional interface that might be defined on a per-FRU basis.
        if (item.find("extraInterfaces") != item.end())
        {
            populateInterfaces(item["extraInterfaces"], interfaces, vpdMap,
                               isSystemVpd);
        }
        objects.emplace(move(object), move(interfaces));
    }

    // Notify PIM
    inventory::callPIM(move(objects));
}

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        using json = nlohmann::json;

        App app{"ibm-read-vpd - App to read IPZ format VPD, parse it and store "
                "in DBUS"};
        string file{};

        app.add_option("-f, --file", file, "File containing VPD (IPZ/KEYWORD)")
            ->required()
            ->check(ExistingFile);

        CLI11_PARSE(app, argc, argv);

        // Make sure that the file path we get is for a supported EEPROM
        ifstream inventoryJson(INVENTORY_JSON);
        auto js = json::parse(inventoryJson);

        if ((js.find("frus") == js.end()) ||
            (js["frus"].find(file) == js["frus"].end()))
        {
            cout << "Device path not in JSON, ignoring" << std::endl;
            return 0;
        }

        Binary vpdVector(getVpdDataInVector(js, file));
        vpdType type = vpdTypeCheck(vpdVector);

        switch (type)
        {
            case IPZ_VPD:
            {
                // Invoking IPZ Vpd Parser
                auto vpdStore = parse(move(vpdVector));
                const Parsed& vpdMap = vpdStore.getVpdMap();
                string preIntrStr = "com.ibm.ipzvpd.";
                // Write it to the inventory
                populateDbus(vpdMap, js, file, preIntrStr);
            }
            break;

            case KEYWORD_VPD:
            {
                // Creating Keyword Vpd Parser Object
                KeywordVpdParser parserObj(move(vpdVector));
                // Invoking KW Vpd Parser
                const auto& kwValMap = parserObj.parseKwVpd();
                string preIntrStr = "com.ibm.kwvpd.KWVPD";
                populateDbus(kwValMap, js, file, preIntrStr);
            }
            break;

            case MEMORY_VPD:
            {
                // Get an object to call API & get the key-value map
                memoryVpdParser vpdParser(move(vpdVector));
                const auto& memKwValMap = vpdParser.parseMemVpd();

                string preIntrStr = "com.ibm.kwvpd.KWVPD";
                // js(define dimm sys path in js), ObjPath(define in JS)
                populateDbus(memKwValMap, js, file, preIntrStr);
            }
            break;

            default:
                throw std::runtime_error("Invalid VPD format");
        }
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
        rc = -1;
    }

    return rc;
}
