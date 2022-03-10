#include "vpd_tool_impl.hpp"

#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace CLI;
using namespace std;
namespace fs = std::filesystem;
using namespace openpower::vpd;
using json = nlohmann::json;

int main(int argc, char** argv)
{
    int rc = 0;
    App app{"VPD Command line tool to dump the inventory and to read and "
            "update the keywords"};

    string objectPath{};
    string recordName{};
    string keyword{};
    string val{};

    auto object =
        app.add_option("--object, -O", objectPath, "Enter the Object Path");
    auto record =
        app.add_option("--record, -R", recordName, "Enter the Record Name");
    auto kw = app.add_option("--keyword, -K", keyword, "Enter the Keyword");
    auto valOption = app.add_option(
        "--value, -V", val,
        "Enter the value. The value to be updated should be either in ascii or "
        "in hex. ascii eg: 01234; hex eg: 0x30313233");
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

    auto writeFlag =
        app.add_flag(
               "--writeKeyword, -w, --updateKeyword, -u",
               "Update the value. { vpd-tool-exe "
               "--writeKeyword/-w/--updateKeyword/-u "
               "--object/-O object-name --record/-R record-name --keyword/-K "
               "keyword-name --value/-V value-to-be-updated }")
            ->needs(object)
            ->needs(record)
            ->needs(kw)
            ->needs(valOption);

    auto forceResetFlag = app.add_flag(
        "--forceReset, -f, -F", "Force Collect for Hardware. { vpd-tool-exe "
                                "--forceReset/-f/-F }");
    auto Hardware = app.add_flag(
        "--Hardware, -H",
        "This is a supplementary flag to write directly to hardware. When the "
        "-H flag is given, User should provide valid hardware/eeprom path (and "
        "not dbus object path) in the -O/--object path.");

    CLI11_PARSE(app, argc, argv);

    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    auto jsObject = json::parse(inventoryJson);

    try
    {
        if (*Hardware)
        {
            if (!fs::exists(objectPath)) // if dbus object path is given or
                                         // invalid eeprom path is given
            {
                string errorMsg = "Invalid EEPROM path : ";
                errorMsg += objectPath;
                errorMsg +=
                    ". The given EEPROM path doesn't exist. Provide valid "
                    "EEPROM path when -H flag is used. Refer help option. ";
                throw runtime_error(errorMsg);
            }
        }
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

        else if (*writeFlag && !*Hardware)
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

        else if (*writeFlag && *Hardware)
        {
            VpdTool vpdToolObj(move(objectPath), move(recordName),
                               move(keyword), move(val));
            rc = vpdToolObj.updateHardware();
        }

        else
        {
            throw runtime_error("One of the valid options is required. Refer "
                                "--help for list of options.");
        }
    }

    catch (const exception& e)
    {
        cerr << e.what();
        rc = -1;
    }

    return rc;
}
