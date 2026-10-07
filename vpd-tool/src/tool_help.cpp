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
                   {"validateRedundantEeprom",
                    &VpdToolHelp::printValidateRedundantEepromHelp}};

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

void VpdToolHelp::printValidateRedundantEepromHelp() const noexcept
{
    try
    {
        std::cout
            << "Operation: validateRedundantEeprom\n\n"
            << "Description:\n"
            << "  Validates a primary EEPROM against its redundant EEPROM copy.\n"
            << "  Only the primary EEPROM path should be provided as input.\n"
            << "  Providing a redundant EEPROM path is not supported.\n\n"
            << "Usage:\n"
            << "  vpd-tool --validateRedundantEeprom -O <EEPROM Path>\n"
            << "  vpd-tool -e -O <EEPROM Path>\n\n"
            << "Return values:\n"
            << "  Success (0): EEPROM validation succeeded. A confirmation message is printed\n"
            << "               to the console.\n"
            << "  Failure :    Error code is returned and error message is displayed on the console.\n"
            << "               Check the system journal logs for more details.\n";

        std::cout << "\nError codes for validate redundant EEPROM operation\n";

        Table errTable(' ', '|', true);
        errTable.addColumn("Code", 8);
        errTable.addColumn("Description", 38);

        const types::TableInputData errData = {
            {"-2", "Invalid input parameter"}, {"-5", "D-Bus call failed"}};

        errTable.print(errData);
    }
    catch (const std::exception& ex)
    {
        std::cerr
            << "Failed to print validate redundant EEPROM help text. Please try again."
            << std::endl;
    }
}
} // namespace vpd
