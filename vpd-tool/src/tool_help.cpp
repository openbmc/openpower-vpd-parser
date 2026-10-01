#include "tool_help.hpp"

#include "tool_constants.hpp"
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
                   {"readKeyword", &VpdToolHelp::printReadKeywordHelp}};

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
    // TODO - Print write keyword help in tabular format using utils::Table
    // class
}

void VpdToolHelp::printReadKeywordHelp() const noexcept
{
    try
    {
        std::cout
            << "Note:\n"
            << "  1. Options in [] are optional.\n"
            << "     If -R is omitted, keyword VPD format is assumed.\n"
            << "  2. If --file is provided, keyword value is saved to the given "
               "file path.\n"
            << "\n";

        utils::Table usageTable(' ', '|', true);
        usageTable.AddColumn("usage", 28);
        usageTable.AddColumn("Description", 46);
        usageTable.AddColumn("Requires", 24);
        usageTable.AddColumn("Example", 50);
        usageTable.AddColumn("Return", 28);

        const types::TableInputData usageData = {
            {"Read keyword",
             "Reads keyword value from DBus for the given record.",
             "-r -O -K -R", "vpd-tool -r -O <obj> -R <rec> -K <kw>",
             "Success: Number of bytes read"},
            {"Read keyword from hardware using -H",
             "Reads data directly from the hardware path provided.",
             "-r -H -O -K [-R]", "vpd-tool -r -H -O <eeprom> -K <kw>",
             "Failure : Error code"},
            {"Save keyword value to file using --file",
             "Reads keyword value and saves it to the file path provided.",
             "-r -O -K [-R] [--file]",
             "vpd-tool -r -O <obj> -K <kw> --file <path>", ""}};

        usageTable.Print(usageData, true);

        std::cout << "\nError codes for the read keyword operation\n";

        utils::Table errTable(' ', '|', true);
        errTable.AddColumn("Code", 8);
        errTable.AddColumn("Description", 38);

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

        errTable.Print(errData);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Failed to print read keyword help: " << ex.what()
                  << std::endl;
    }
}
} // namespace vpd
