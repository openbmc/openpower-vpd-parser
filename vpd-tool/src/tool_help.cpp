#include "tool_help.hpp"

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
        flagToHelp{
            {"writeKeyword", &VpdToolHelp::printWriteKeywordHelp},
        };

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

        utils::Table helpTable(' ', '|', true);
        helpTable.AddColumn("Usage", 25);
        helpTable.AddColumn("Description", 60);
        helpTable.AddColumn("Requires", 25);
        helpTable.AddColumn("Example", 60);
        helpTable.AddColumn("Return", 40);

        const types::TableInputData helpData{
            {"Write keyword using -V",
             "Updates value on both hardware and D-Bus.", "-w -O -K -R -V",
             "vpd-tool -w -O <object path> -R <Record Name> -K <Keyword Name> -V <Value>",
             "Success: Number of bytes written. Failure: Error code and error message is displayed on the console"},
            {"Write keyword using --file",
             "File should contain the value to be written. Updates value on both hardware and D-Bus.",
             "-w --file -O -K -R",
             "vpd-tool -w --file <File path> -O <Object Name> -R <Record Name> -K <Keyword Name>",
             "Same as above"},
            {"Write keyword to hardware using -V",
             "CAUTION: -H is developer-only option. Updates data only on the hardware path provided. Data is not synced between primary and redundant EEPROMs.",
             "-w -H -O -K [-R] -V",
             "vpd-tool -w -H -O <EEPROM path> -R <Record Name> -K <Keyword Name> -V <Value>",
             "Same as above"},
            {"Write keyword to hardware using --file",
             "File should contain the value to be written. Updates data only on the hardware path provided. CAUTION: -H is developer-only option. Data is not synced between primary and redundant EEPROMs.",
             "-w -H --file -O -K [-R]",
             "vpd-tool -w -H --file <File path> -O <EEPROM path> -R <Record Name> -K <Keyword Name>",
             "Same as above"},
        };

        helpTable.Print(helpData, true);

        std::cout << "\nError codes for the write keyword operation\n";

        utils::Table errTable(' ', '|', true);
        errTable.AddColumn("Code", 8);
        errTable.AddColumn("Description", 36);

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

        errTable.Print(errData);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Failed to print write keyword help. Please try again."
                  << std::endl;
    }
}
} // namespace vpd
