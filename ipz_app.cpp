#include "config.h"

#include "defines.hpp"
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

static void populateInterfaces(const nlohmann::json& js,
                               inventory::InterfaceMap& interfaces,
                               const Parsed& vpdMap)
{
    for (const auto& ifs : js.items())
    {
        const string& inf = ifs.key();
        inventory::PropertyMap props;

        for (const auto& itr : ifs.value().items())
        {
            const string& rec = itr.value().value("recordName", "");
            const string& kw = itr.value().value("keywordName", "");

            if (!rec.empty() && !kw.empty() && vpdMap.count(rec) &&
                vpdMap.at(rec).count(kw))
            {
                props.emplace(itr.key(), string(vpdMap.at(rec).at(kw).begin(),
                                                vpdMap.at(rec).at(kw).end()));
            }
        }
        interfaces.emplace(inf, move(props));
    }
}

static void populateDbus(Store& vpdStore, nlohmann::json& js,
                         const string& objectPath, const string& filePath)
{
    inventory::InterfaceMap interfaces;
    inventory::ObjectMap objects;
    sdbusplus::message::object_path object(objectPath);
    const auto& vpdMap = vpdStore.getVpdMap();
    string preIntrStr = "com.ibm.ipzvpd.";

    // Each record in the VPD becomes an interface and all keywords within the
    // record are properties under that interface.
    for (const auto& record : vpdMap)
    {
        inventory::PropertyMap prop;
        for (auto kwVal : record.second)
        {
            std::vector<uint8_t> vec(kwVal.second.begin(), kwVal.second.end());
            std::string kw = kwVal.first;
            if (kw[0] == '#')
            {
                kw = std::string("PD_") + kw[1];
            }
            prop.emplace(move(kw), move(vec));
        }
        interfaces.emplace(preIntrStr + record.first, move(prop));
    }

    // Populate interfaces and properties that are common to every FRU
    // and additional interface that might be defined on a per-FRU basis.
    if (js.find("commonInterfaces") != js.end())
    {
        populateInterfaces(js["commonInterfaces"], interfaces, vpdMap);
    }
    if (js["frus"][filePath].find("extraInterfaces") !=
        js["frus"][filePath].end())
    {
        populateInterfaces(js["frus"][filePath]["extraInterfaces"], interfaces,
                           vpdMap);
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
        using namespace CLI;
        using json = nlohmann::json;

        App app{"ibm-read-vpd - App to read IPZ format VPD, parse it and store "
                "in DBUS"};
        string file{};

        app.add_option("-f, --file", file, "File containing VPD in IPZ format")
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

        ifstream vpdFile(file, ios::binary);
        Binary vpd((istreambuf_iterator<char>(vpdFile)),
                   istreambuf_iterator<char>());

        // Use ipz vpd Parser
        auto vpdStore = parse(move(vpd));

        // Write it to the inventory
        populateDbus(vpdStore, js, objectPath, file);
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
        rc = -1;
    }

    return rc;
}
