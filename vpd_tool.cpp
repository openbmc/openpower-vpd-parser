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
    enum constants::FileType fileType = constants::FileType::UNKNOWN;

    auto object =
        app.add_option("--object, -O", objectPath, "Enter the Object Path");
    auto record =
        app.add_option("--record, -R", recordName, "Enter the Record Name");
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
               "keyword-name --value/-V value-to-be-updated }")
            ->needs(object)
            ->needs(record)
            ->needs(kw)
            ->needs(valOption);

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

    auto mfgClean =
        app.add_flag("--mfgClean", "Flag to clean and reset specific keywords "
                                   "on system VPD to its default value.");

    auto confirm =
        app.add_flag("--yes", "Using this flag with --mfgClean option, assumes "
                              "yes to proceed without confirmation.");

    auto textFileFlag = app.add_flag(
        "--textInHex, -t",
        "This flag can be used with read and write options. In read option, "
        "the read data is dumped into a text file (in 2 digit hex format)."
        "In write option, the user has to provide the text file path in "
        "--value and provite -t flag to indicate that the value given in "
        "--value is of type text. The text file given under --value should be "
        "in 2 digit hex format.");

    CLI11_PARSE(app, argc, argv);

    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    auto jsObject = json::parse(inventoryJson);

    try
    {
        if (*textFileFlag)
        {
            fileType = constants::FileType::TEXT_IN_HEX;
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
                               move(keyword));
            vpdToolObj.readKeyword(fileType);
        }

        else if (*writeFlag && !*Hardware)
        {
            VpdTool vpdToolObj(move(objectPath), move(recordName),
                               move(keyword), move(val));
            rc = vpdToolObj.updateKeyword(fileType);
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
            rc = vpdToolObj.updateHardware(offset, fileType);
        }
        else if (*readFlag && *Hardware)
        {
            VpdTool vpdToolObj(move(objectPath), move(recordName),
                               move(keyword));
            vpdToolObj.readKwFromHw(offset, fileType);
        }
        else if (*fixSystemVPDFlag)
        {
            VpdTool vpdToolObj;
            rc = vpdToolObj.fixSystemVPD();
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

        if (*Hardware)
        {
            std::cerr << "\nDid you provide a valid offset? By default VPD "
                         "offset is taken as 0. To input offset, use --seek. "
                         "Refer vpd-tool help.";
        }
        rc = -1;
    }

    return rc;
}