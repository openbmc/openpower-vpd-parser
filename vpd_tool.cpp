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
    uint32_t offset = 0;

    auto object = app.add_option("--object, -O", objectPath,
                                 "Enter the Object Path");
    auto record = app.add_option("--record, -R", recordName,
                                 "Enter the Record Name");
    auto kw = app.add_option("--keyword, -K", keyword, "Enter the Keyword");
    auto valOption = app.add_option(
        "--value, -V", val,
        "Enter the value. The value to be updated should be either in ascii or "
        "in hex. ascii eg: 01234; hex eg: 0x30313233");
    app.add_option("--seek, -s", offset,
                   "User can provide VPD offset using this option. Default "
                   "offset value is 0. Using --seek is optional and is valid "
                   "only while using --Hardware/-H option.");

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
               "keyword-name --value/-V (or) --file }. Value can be given "
               "directly via console using --value or via file using --file")
            ->needs(object)
            ->needs(record)
            ->needs(kw);

    auto fileOption = app.add_option(
        "--file", val,
        "Enter the file name with its absolute path. This option can be used "
        "in read and write operations. When used in read, the read value will "
        "be saved to this file and when used in write, the value to be written "
        "will be taken from this file.");

    auto forceResetFlag =
        app.add_flag("--forceReset, -f, -F",
                     "Force Collect for Hardware. CAUTION: Developer Only "
                     "Option. { vpd-tool-exe --forceReset/-f/-F }");

    auto Hardware = app.add_flag(
        "--Hardware, -H",
        "This is a supplementary flag to read/write directly from/to hardware. "
        "User should provide valid hardware/eeprom path (and not dbus object "
        "path) in the -O/--object path. CAUTION: Developer Only Option");

    auto fixSystemVPDFlag = app.add_flag(
        "--fixSystemVPD", "Use this option to interactively fix critical "
                          "system VPD keywords {vpd-tool-exe --fixSystemVPD}");

    auto mfgClean = app.add_flag("--mfgClean",
                                 "Flag to clean and reset specific keywords "
                                 "on system VPD to its default value.");

    auto confirm =
        app.add_flag("--yes", "Using this flag with --mfgClean option, assumes "
                              "yes to proceed without confirmation.");

    CLI11_PARSE(app, argc, argv);

    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    auto jsObject = json::parse(inventoryJson);

    try
    {
        if ((*kw) && (keyword.size() != 2))
        {
            throw runtime_error("Keyword " + keyword + " not supported.");
        }

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

        if (*writeFlag)
        {
            if ((!*fileOption) && (!*valOption))
            {
                throw runtime_error("Please provide the data that needs to be "
                                    "updated. Use --value/--file to "
                                    "input data. Refer --help.");
            }

            if ((*fileOption) && (!fs::exists(val)))
            {
                throw runtime_error("Please provide a valid file with absolute "
                                    "path in --file.");
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

        else if (*readFlag && !*Hardware)
        {
            VpdTool vpdToolObj(move(objectPath), move(recordName),
                               move(keyword), move(val));
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
            // Force reset the BMC only if the CEC is powered OFF.
            if (getPowerState() ==
                "xyz.openbmc_project.State.Chassis.PowerState.Off")
            {
                VpdTool vpdToolObj;
                vpdToolObj.forceReset(jsObject);
            }
            else
            {
                std::cerr << "The chassis power state is not Off. Force reset "
                             "operation is not allowed.";
                return -1;
            }
        }

        else if (*writeFlag && *Hardware)
        {
            VpdTool vpdToolObj(move(objectPath), move(recordName),
                               move(keyword), move(val));
            rc = vpdToolObj.updateHardware(offset);
        }
        else if (*readFlag && *Hardware)
        {
            VpdTool vpdToolObj(move(objectPath), move(recordName),
                               move(keyword), move(val));
            vpdToolObj.readKwFromHw(offset);
        }
        else if (*fixSystemVPDFlag)
        {
            std::string backupEepromPath;
            std::string backupInvPath;
            findBackupVPDPaths(backupEepromPath, backupInvPath, jsObject);
            VpdTool vpdToolObj;

            if (backupEepromPath.empty())
            {
                rc = vpdToolObj.fixSystemVPD();
            }
            else
            {
                rc = vpdToolObj.fixSystemBackupVPD(backupEepromPath,
                                                   backupInvPath);
            }
        }
        else if (*mfgClean)
        {
            if (!*confirm)
            {
                std::string confirmation{};
                std::cout << "\nThis option resets some of the system VPD "
                             "keywords to their default values. Do you really "
                             "wish to proceed further?[yes/no]: ";
                std::cin >> confirmation;

                if (confirmation != "yes")
                {
                    return 0;
                }
            }
            VpdTool vpdToolObj;
            rc = vpdToolObj.cleanSystemVPD();
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
