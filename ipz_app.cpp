#include "CLI/CLI.hpp"
#include "defines.hpp"
#include "parser.hpp"
#include "utils.hpp"
#include "write.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace std;
using namespace openpower::vpd;

void populateDbus(Store& vpdStore)
{
    string objectPath{};
    inventory::InterfaceMap interfaces;
    inventory::ObjectMap objects;
    sdbusplus::message::object_path object(objectPath);

    Parsed vpdData = vpdStore.get();
    constexpr auto preIntrStr = "com.ibm.interface.";

    // loop through the record and append interface to the record
    for (const auto& rec_kw : vpdData)
    {
        inventory::PropertyMap prop;
        for (auto kw_val : rec_kw.second)
        {
            std::vector<uint8_t> vec(kw_val.second.begin(),
                                     kw_val.second.end());
            std::string kw = kw_val.first;
            if (kw[0] == '#')
            {
                kw = std::string("PD_") + kw[1];
            }
            prop.emplace(move(kw), move(vec));
        }
        interfaces.emplace(preIntrStr + rec_kw.first, move(prop));
    }

    objects.emplace(move(object), move(interfaces));

    // Notify method call
    inventory::callPIM(move(objects));
}

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        using namespace CLI;
        using namespace openpower::vpd;

        App app{"ipz-read-vpd - App to read IPZ format VPD, parse it and store "
                "in DBUS"};
        string file{};

        app.add_option("-f, --file", file, "File containing VPD in IPZ format")
            ->required()
            ->check(ExistingFile);

        CLI11_PARSE(app, argc, argv);

        ifstream vpdFile(file, ios::binary);
        Binary vpd((istreambuf_iterator<char>(vpdFile)),
                   istreambuf_iterator<char>());

        // Use ipz vpd Parser
        auto vpdStore = parse(move(vpd));

        // Write it to the D-bus, Create an object Map
        populateDbus(vpdStore);
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
    }

    return rc;
}
