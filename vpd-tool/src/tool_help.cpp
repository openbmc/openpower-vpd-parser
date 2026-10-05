#include "tool_help.hpp"

#include "tool_table.hpp"
#include "tool_utils.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vpd
{

bool VpdToolHelp::printHelp(int argc, char** argv) const
{
    // Print generic help when no operation are provided.
    if (argc == 1)
    {
        std::cout << "No operation specified. Please provide an opeartion."
                  << std::endl;
        printGenericHelp();
        return true;
    }

    const std::vector<std::string> args(argv + 1, argv + argc);

    // Return early if --help / -h is not present.
    const bool wantHelp = std::ranges::any_of(args, [](const auto& arg) {
        return arg == "--help" || arg == "-h";
    });

    if (!wantHelp)
    {
        return false;
    }

    // Map every recognised operation flag to its print method.
    const std::unordered_map<std::string,
                             std::function<void(const VpdToolHelp*)>>
        flagToHelp{{"writeKeyword", &VpdToolHelp::printWriteKeywordHelp},
                   {"readKeyword", &VpdToolHelp::printReadKeywordHelp},
                   {"dumpInventory", &VpdToolHelp::printDumpInventoryHelp}};

    for (const auto& arg : args)
    {
        if (const auto it = flagToHelp.find(arg); it != flagToHelp.end())
        {
            it->second(this);
            return true;
        }
    }

    // No operation specified: print generic help.
    printGenericHelp();
    return true;
}

void VpdToolHelp::printGenericHelp() const noexcept
{
    std::cout
        << "VPD Command Line Tool\n\n"
        << "Operations:\n"
        << "  readKeyword                   Read a keyword value from DBus or hardware\n"
        << "  writeKeyword                  Write a keyword value to DBus and/or hardware\n"
        << "  dumpInventory                 Dump inventory information\n"
        << "  validateRedundantEeprom       Validate an EEPROM against its redundant copy\n"
        << "  enterSplitMode                Configure the system to operate in split mode\n"
        << "  exitSplitMode                 Configure the system to exit split mode\n"
        << "\n"
        << "Use 'vpd-tool <operation> --help' for more information on a specific operation.\n";
}

void VpdToolHelp::printWriteKeywordHelp() const noexcept
{
    try
    {
        std::cout
            << "Note:\n"
            << "  1. Options in [] are optional.\n"
            << "     If -R is omitted, keyword VPD format is assumed.\n"
            << "  2. Keyword value should be in ASCII or hexadecimal format.\n"
            << "     ASCII example: 01234; hexadecimal example: 0x30313233\n\n";

        Table helpTable(' ', '|', true);
        helpTable.addColumn("Usage", 25);
        helpTable.addColumn("Description", 60);
        helpTable.addColumn("Requires", 25);
        helpTable.addColumn("Example", 60);
        helpTable.addColumn("Return", 50);

        const types::TableInputData helpData{
            {"Write keyword using -V",
             "Updates value on both hardware and D-Bus.", "-w -O -K -R -V",
             "vpd-tool -w -O <Object Path> -R <Record Name> -K <Keyword Name> -V <Value>",
             "Success: Number of bytes written is returned. Failure: Error code is returned, and the error message is displayed on the console."},
            {"Write keyword using --file",
             "File should contain the value to be written. Updates value on both hardware and D-Bus.",
             "-w --file -O -K -R",
             "vpd-tool -w --file <File Path> -O <Object Path> -R <Record Name> -K <Keyword Name>",
             "Same as above"},
            {"Write keyword to hardware using -V",
             "CAUTION: -H is developer-only option. Updates data only on the hardware path provided. Data is not synced between primary and redundant EEPROMs.",
             "-w -H -O -K [-R] -V",
             "vpd-tool -w -H -O <EEPROM Path> -R <Record Name> -K <Keyword Name> -V <Value>",
             "Same as above"},
            {"Write keyword to hardware using --file",
             "File should contain the value to be written. Updates data only on the hardware path provided. CAUTION: -H is developer-only option. Data is not synced between primary and redundant EEPROMs.",
             "-w -H --file -O -K [-R]",
             "vpd-tool -w -H --file <File path> -O <EEPROM path> -R <Record Name> -K <Keyword Name>",
             "Same as above"},
        };

        helpTable.print(helpData, true);

        std::cout << "\nError codes for the write keyword operation\n";

        Table errTable(' ', '|', true);
        errTable.addColumn("Code", 8);
        errTable.addColumn("Description", 36);

        const types::TableInputData errData{
            {"-2", "Input parameter(s) are invalid"},
            {"-3", "Record name is not provided"},
            {"-4", "Keyword value is not provided"},
            {"-5", "DBus call failed"},
            {"-6", "File system error"},
            {"-7", "File not found"},
            {"-8", "Standard exception occurred"},
            {"-9", "JSON parse error"},
            {"-10", "EEPROM path not found"},
            {"-11", "Empty file"},
            {"-12", "Keyword name is not provided"},
        };

        errTable.print(errData);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Failed to print write keyword help. Please try again."
                  << std::endl;
    }
}

void VpdToolHelp::printReadKeywordHelp() const noexcept
{
    try
    {
        std::cout << "Note:\n"
                  << "  Options in [] are optional.\n"
                  << "  If -R is omitted, keyword VPD format is assumed.\n";

        Table usageTable(' ', '|', true);
        usageTable.addColumn("Usage", 28);
        usageTable.addColumn("Description", 46);
        usageTable.addColumn("Requires", 24);
        usageTable.addColumn("Example", 50);
        usageTable.addColumn("Return", 50);

        const types::TableInputData usageData = {
            {"Read keyword",
             "Reads keyword value from DBus for the given record.",
             "-r -O -K -R",
             "vpd-tool -r -O <Object Path> -R <Record Name> -K <Keyword name>",
             "Success: Number of bytes read is returned, and the keyword value is displayed on the console."
             "Failure: Error code is returned, and the error message is displayed on the console."},
            {"Read keyword using -H",
             "Reads keyword value from hardware. Reads data directly from the hardware path provided.",
             "-r -H -O -K [-R]",
             "vpd-tool -r -H -O <EEPROM Path> -R <Record Name> -K <Keyword Name>",
             "Same as above"},
            {"Read keyword using --file",
             "Reads keyword value and saves it to the file path provided.",
             "-r -O -K -R --file",
             "vpd-tool -r -O <Object path> -R <Record name> -K <Keyword Name> --file <File path>",
             "Same as above"},
            {"Read keyword using -H, --file",
             "Reads keyword value from hardware and saves it to the file path provided.",
             "-r -H -O -K [-R] --file",
             "vpd-tool -r -O <EEPROM Path> -R <Record Name> -K <Keyword Name> --file <File path>",
             "Same as above"}};

        usageTable.print(usageData, true);

        std::cout << "\nError codes for the read keyword operation\n";

        Table errTable(' ', '|', true);
        errTable.addColumn("Code", 8);
        errTable.addColumn("Description", 38);

        const types::TableInputData errData = {
            {"-2", "Input parameter(s) are invalid"},
            {"-3", "Record name is not provided"},
            {"-5", "DBus call failed"},
            {"-6", "File system error"},
            {"-7", "File not found"},
            {"-8", "Standard exception occurred"},
            {"-9", "JSON parse error"},
            {"-10", "EEPROM path not found"},
            {"-11", "Empty file"},
            {"-12", "Keyword name is not provided"}};

        errTable.print(errData);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Failed to print read keyword help. Please try again"
                  << std::endl;
    }
}

void VpdToolHelp::printDumpInventoryHelp() const noexcept
{
    try
    {
        Table usageTable(' ', '|', true);
        usageTable.addColumn("Usage", 25);
        usageTable.addColumn("Description", 60);
        usageTable.addColumn("Requires", 20);
        usageTable.addColumn("Example", 25);
        usageTable.addColumn("Return", 53);

        const types::TableInputData usageData = {
            {"Dump inventory using -i",
             "Dumps inventory data for the FRUs present on the system to the console in JSON format.",
             "-i", "vpd-tool -i",
             "Success: Dumps inventory data to the console. "
             "Failure: Error code is returned and the error message is displayed on the console."},
            {"Dump inventory using -t",
             "Dumps inventory data for the FRUs present on the system to the console in tabular format.",
             "-i -t", "vpd-tool -i -t", "Same as above"},
            {"Dump chassis inventory using -c, -N",
             "Dumps inventory data for the FRUs present on the specified chassis to the console in JSON format.",
             "-i -c -N", "vpd-tool -i -c -N <chassis_id>",
             "Success: Dumps chassis-specific inventory data. "
             "Failure: Error code is returned and the error message is displayed on the console."},
            {"Dump chassis inventory using -c, -N, -t",
             "Dumps inventory data for the FRUs present on the specified chassis to the console in tabular format.",
             "-i -c -t -N", "vpd-tool -i -c -t -N <chassis_id>",
             "Same as above"},
        };

        usageTable.print(usageData, true);

        std::cout << "\nError codes for the dump inventory operation\n";

        Table errorTable(' ', '|', true);
        errorTable.addColumn("Code", 8);
        errorTable.addColumn("Description", 40);

        const types::TableInputData errorData = {
            {"-2", "Input parameter(s) are invalid"},
            {"-5", "DBus call failed"},
            {"-8", "Standard exception occurred"},
            {"-9", "JSON parse error"},
            {"-15", "Chassis Id is not provided"},
            {"-17", "Inventory object not found"},
        };

        errorTable.print(errorData);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Failed to print dump inventory help. Please try again."
                  << std::endl;
    }
}
} // namespace vpd
