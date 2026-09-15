#include "tool_help.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vpd
{

bool VpdToolHelp::printHelp(int argc, char** argv) const noexcept
{
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
        << "Use 'vpd-tool <operation> --help' for detailed usage.\n";
}

void VpdToolHelp::printWriteKeywordHelp() const noexcept
{
    // TODO - print write keyword help
}
} // namespace vpd
