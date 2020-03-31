#include "vpd_tool_impl.hpp"

#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>

using namespace CLI;
using namespace std;
using json = nlohmann::json;

int main(int argc, char** argv)
{
    int rc = 0;
    App app{"VPD Command line tool to dump the inventory and to read and "
            "update the keywords"};

    string objectPath;
    string recordName;
    string keyword;
    string val;

    auto object =
        app.add_option("--object, -O", objectPath, "Enter the Object Path");
    auto record =
        app.add_option("--record, -R", recordName, "Enter the Record Name");
    auto kw = app.add_option("--keyword, -K", keyword, "Enter the Keyword");
    auto valOption = app.add_option(
        "--value, -V", val,
        "Enter the value. The value to be updated should be either in ascii or "
        "in hex. ascii eg: 01234; hex eg: 0x30313233");

    auto dumpObjFlag = app.add_flag("--dumpObject, -o",
                                    "Dump the given object from the inventory")
                           ->needs(object);

    auto dumpInvFlag =
        app.add_flag("--dumpInventory, -i", "Dump all the inventory objects");

    auto readFlag =
        app.add_flag("--readKeyword, -r", "Read the data of the given keyword")
            ->needs(object)
            ->needs(record)
            ->needs(kw);
    auto writeFlag = app.add_flag("--writeKeyword, -w, --updateKeyword, -u",
                                  "Update the value")
                         ->needs(object)
                         ->needs(record)
                         ->needs(kw)
                         ->needs(valOption);
    auto forceResetFlag =
        app.add_flag("--forceReset, -f, -F", "Force Collect for Hardware");

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

        else if (*writeFlag)
        {
            VpdTool vpdToolObj(move(objectPath), move(recordName),
                               move(keyword), move(val));
            rc = vpdToolObj.updateKeyword();
        }

        else if (*forceResetFlag)
        {
            VpdTool vpdToolObj;
            vpdToolObj.forceReset(jsObject);
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

    return rc;
}
