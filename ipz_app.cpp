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
        string objectPath{};

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
        inventory::InterfaceMap interfaces;
        inventory::PropertyMap prop;
        inventory::ObjectMap objects;
        sdbusplus::message::object_path object(objectPath);
        Parsed st_bin = vpdStore.get();
        string preIntrStr = "com.ibm.interface.";

        // loop through the record and append interface to the record
        for (auto rec_kw : st_bin)
        {
            string(rec_kw.first).insert(0, preIntrStr);

            for (auto kw_val : rec_kw.second)
            {
                prop[kw_val.first] = kw_val.second;
            }
            interfaces.emplace(rec_kw.first, prop);
        }

        objects.emplace(move(object), move(interfaces));

        // Notify method call
        inventory::callPIM(move(objects));
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
    }

    return rc;
}
