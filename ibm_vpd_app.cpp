#include "config.h"

#include "CLI/CLI.hpp"
#include "defines.hpp"
#include "ibm_vpd_type_check.hpp"
#include "keyword_vpd_parser.hpp"
#include "parser.hpp"
#include "utils.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>

using namespace std;
using namespace openpower::vpd;
using namespace CLI;
using namespace vpd::keyword::parser;

template <typename T>
static void call_func(const T& map, string preIntrStr,
                      inventory::InterfaceMap& interfaces)
{
    inventory::PropertyMap prop;

    for (const auto kwVal : map)
    {
        std::vector<uint8_t> vec(kwVal.second.begin(), kwVal.second.end());

        std::string kw = kwVal.first;

        if constexpr (std::is_same<T, Parsed>::value)
        {
            if (kw[0] == '#')
            {
                kw = std::string("PD_") + kw[1];
            }
        }
        prop.emplace(move(kw), move(vec));
    }

    interfaces.emplace(preIntrStr, move(prop));
}

template <typename T>
static void populateInterfaces(const nlohmann::json& js,
                               inventory::InterfaceMap& interfaces,
                               const T& ibmVpdMap)
{
    for (const auto& ifs : js.items())
    {
        const string& inf = ifs.key();
        inventory::PropertyMap props;

        for (const auto& itr : ifs.value().items())
        {
            const string& kw = itr.value().value("keywordName", "");
            string rec = itr.value().value("recordName[0]", "");

            if constexpr (std::is_same<T, Parsed>::value)
            {
                if (!rec.empty() && !kw.empty() &&
                    ibmVpdMap.at(rec).count(kw) && ibmVpdMap.count(rec))
                {
                    props.emplace(itr.key(),
                                  string(ibmVpdMap.at(rec).at(kw).begin(),
                                         ibmVpdMap.at(rec).at(kw).end()));
                }
            }
            else if constexpr (std::is_same<T, KeywordVpdMap>::value)
            {
                rec = itr.value().value("recordName[1]", "");

                if (!rec.empty() && !kw.empty() && ibmVpdMap.count(kw))
                {
                    props.emplace(itr.key(), string(ibmVpdMap.at(kw).begin(),
                                                    ibmVpdMap.at(kw).end()));
                }
            }
        }
        interfaces.emplace(inf, move(props));
    }
}

template <typename T>
static void populateDbus(const T& ibmVpdMap, nlohmann::json& js,
                         const string& objectPath, const string& filePath,
                         string preIntrStr)
{
    inventory::InterfaceMap interfaces;
    inventory::ObjectMap objects;
    sdbusplus::message::object_path object(objectPath);
    inventory::PropertyMap prop;

    if constexpr (std::is_same<T, Parsed>::value)
    {
        // Each record in the VPD becomes an interface and all keyword
        // within the record are properties under that interface.
        for (const auto& record : ibmVpdMap)
        {
            call_func(record.second, preIntrStr + record.first, interfaces);
        }
    }
    else if constexpr (std::is_same<T, KeywordVpdMap>::value)
    {
        call_func(ibmVpdMap, preIntrStr, interfaces);
    }

    // Populate interfaces and properties that are common to every FRU
    // and additional interface that might be defined on a per-FRU basis.

    if (js.find("commonInterfaces") != js.end())
    {
        populateInterfaces(js["commonInterfaces"], interfaces, ibmVpdMap);
    }
    if (js["frus"][filePath].find("extraInterfaces") !=
        js["frus"][filePath].end())
    {
        populateInterfaces(js["frus"][filePath]["extraInterfaces"], interfaces,
                           ibmVpdMap);
    }
    objects.emplace(move(object), move(interfaces));

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

        app.add_option("-f, --file", file,
                       "File containing IBM VPD (IPZ/KEYWORD)")
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

        const string& objectPath = js["frus"][file].value("inventoryPath", "");

        if (objectPath.empty())
        {
            throw std::runtime_error("Could not find D-Bus object path in "
                                     "inventory JSON");
        }

        // Open the file in binary mode
        ifstream ibmVpdFile(file, ios::binary);
        // Read the content of the binary file into a vector
        Binary ibmVpdVector((istreambuf_iterator<char>(ibmVpdFile)),
                            istreambuf_iterator<char>());

        ibmVpdType type = ibmVpdTypeCheck(ibmVpdVector);

        switch (type)
        {
            case IPZ_VPD:
            {
                // Invoking IPZ Vpd Parser
                auto vpdStore = parse(move(ibmVpdVector));
                const Parsed& vpdMap = vpdStore.getVpdMap();
                string preIntrStr = "com.ibm.ipzvpd.";
                // Write it to the inventory
                populateDbus(vpdMap, js, objectPath, file, preIntrStr);
            }
            break;

            case Keyword_VPD:
            {
                // Creating Keyword Vpd Parser Object
                KeywordVpdParser parserObj(move(ibmVpdVector));
                // Invoking KW Vpd Parser
                const KeywordVpdMap kwValMap = parserObj.parseKwVpd();
                string preIntrStr = "com.ibm.kwvpd.KWVPD";
                populateDbus(kwValMap, js, objectPath, file, preIntrStr);
            }
            break;
            default:
                throw std::runtime_error("Invalid IBM VPD format");
        }
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
        rc = -1;
    }

    return rc;
}
