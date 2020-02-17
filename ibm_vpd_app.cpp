#include "config.h"

#include "defines.hpp"
#include "ibm_vpd_type_check.hpp"
#include "keyword_vpd_parser.hpp"
#include "parser.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>
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

/** @brief Encodes a keyword for D-Bus.
 */
static string encodeKeyword(const string& kw, const string& encoding)
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
 */
template <typename T>
static void populateInterfaces(const nlohmann::json& js,
                               inventory::InterfaceMap& interfaces,
                               const T& vpdMap)
{
    for (const auto& ifs : js.items())
    {
        const string& inf = ifs.key();
        inventory::PropertyMap props;

        for (const auto& itr : ifs.value().items())
        {
            // check if the Value is boolean or object
            if (itr.value().is_boolean())
            {
                props.emplace(itr.key(), itr.value().get<bool>());
            }
            else if (itr.value().is_object())
            {
                const string& rec = itr.value().value("recordName", "");
                const string& kw = itr.value().value("keywordName", "");
                const string& encoding = itr.value().value("encoding", "");

                if constexpr (std::is_same<T, Parsed>::value)
                {
                    if (!rec.empty() && !kw.empty() &&
                        vpdMap.at(rec).count(kw) && vpdMap.count(rec))
                    {
                        auto encoded =
                            encodeKeyword(vpdMap.at(rec).at(kw), encoding);
                        props.emplace(itr.key(), encoded);
                    }
                }
                else if constexpr (std::is_same<T, KeywordVpdMap>::value)
                {
                    if (!kw.empty() && vpdMap.count(kw))
                    {
                        auto prop =
                            string(vpdMap.at(kw).begin(), vpdMap.at(kw).end());
                        auto encoded = encodeKeyword(prop, encoding);
                        props.emplace(itr.key(), encoded);
                    }
                }
            }
        }
        interfaces.emplace(inf, move(props));
    }
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
        }

        // Populate interfaces and properties that are common to every FRU
        // and additional interface that might be defined on a per-FRU basis.

        if (item.find("commonInterfaces") != item.end())
        {
            populateInterfaces(item["commonInterfaces"], interfaces, vpdMap);
        }
        if (item.find("extraInterfaces") != item.end())
        {
            populateInterfaces(item["extraInterfaces"], interfaces, vpdMap);
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
            throw std::runtime_error("Device path missing in inventory JSON");
        }

        // Open the file in binary mode
        ifstream vpdFile(file, ios::binary);
        // Read the content of the binary file into a vector
        Binary vpdVector((istreambuf_iterator<char>(vpdFile)),
                         istreambuf_iterator<char>());

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
