#include "vpd_tool_impl.hpp"

#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>

using namespace CLI;
using namespace std;

int main(int argc, char** argv)
{
    App app{"VPD Command line tool to dump the inventory and to read and "
            "update the keywords"};

    string objectPath{};
    string recordName{};
    string keyword{};

    auto object =
        app.add_option("--object, -O", objectPath, "Enter the Object Path");
    auto record =
        app.add_option("--record, -R", recordName, "Enter the Record Name");
    auto kw = app.add_option("--keyword, -K", keyword, "Enter the Keyword");

    auto dumpObjFlag =
        app.add_flag("--dumpObject, -o",
                     "Dump the given object from the inventory. { "
                     "vpd-tool-exe --dumpObject/-o --object/-O object-name }")
            ->needs(object);

    auto dumpInvFlag = app.add_flag(
        "--dumpInventory, -i", "Dump all the inventory objects. { vpd-tool-exe "
                               "--dumpInventory/-i }");

    auto readFlag =
        app.add_flag("--readKeyword, -r",
                     "Read the data of the given keyword. { "
                     "vpd-tool-exe --readKeyword/-r --object/-O "
                     "\"object-name\" --record/-R \"record-name\" --keyword/-K "
                     "\"keyword-name\" }")
            ->needs(object)
            ->needs(record)
            ->needs(kw);

    CLI11_PARSE(app, argc, argv);

    ifstream inventoryJson(INVENTORY_JSON);
    auto jsObject = json::parse(inventoryJson);

    try
    {
        if (*dumpObjFlag)
        {
            VpdTool vpdToolObj(move(objectPath));
            vpdToolObj.dumpObject(jsObject);
        }

        else if (*dumpInvFlag)
        {
            VpdTool vpdToolObj;
            vpdToolObj.dumpInventory(jsObject);
        }

        else if (*readFlag)
        {
            VpdTool vpdToolObj(move(objectPath), move(recordName),
                               move(keyword));
            vpdToolObj.readKeyword();
        }

        else
        {
            throw runtime_error("One of the valid options is required. Refer "
                                "--help for list of options.");
        }
    }

    catch (exception& e)
    {
        cerr << e.what();
    }

    return 0;
}
