#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vpd
{

/**
 * @brief Class to handle operation-specific help for vpd-tool.
 *
 * The private methods hold the per-operation help content. The single
 * public entry point printHelp() inspects argc/argv and dispatches to
 * the appropriate private method when an operation flag is combined
 * with --help / -h.
 */
class VpdToolHelp
{
  private:
    /** @brief Print help for the writeKeyword / updateKeyword operation. */
    void printWriteKeywordHelp() const {}

    /** @brief Print help for the readKeyword operation. */
    void printReadKeywordHelp() const {}

    /** @brief Print help for the dumpInventory operation. */
    void printDumpInventoryHelp() const {}

    /** @brief Print help for the validateRedundantEeprom operation. */
    void printValidateEepromHelp() const {}

    /** @brief Print help for the enterSplitMode / exitSplitMode operations. */
    void printSplitModeHelp() const {}

  public:
    /**
     * @brief Print operation-specific help for the given operation flag.
     *
     * Scans argv for an operation flag (e.g. -w, -r, -i, -e,
     * --enterSplitMode, --exitSplitMode) combined with --help / -h and
     * dispatches to the corresponding private print method. If no operation
     * flag is found alongside --help, the method returns false and lets
     * CLI11 handle the generic help output.
     *
     * @param[in] argc - Argument count (same as main's argc).
     * @param[in] argv - Argument vector (same as main's argv).
     *
     * @return true if operation-specific help was printed (caller must exit 0),
     *         false otherwise.
     */
    bool printHelp(int argc, char** argv) const
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
                {"-w", &VpdToolHelp::printWriteKeywordHelp},
                {"--writeKeyword", &VpdToolHelp::printWriteKeywordHelp},
                {"-u", &VpdToolHelp::printWriteKeywordHelp},
                {"--updateKeyword", &VpdToolHelp::printWriteKeywordHelp},
                {"-r", &VpdToolHelp::printReadKeywordHelp},
                {"--readKeyword", &VpdToolHelp::printReadKeywordHelp},
                {"-i", &VpdToolHelp::printDumpInventoryHelp},
                {"--dumpInventory", &VpdToolHelp::printDumpInventoryHelp},
                {"-e", &VpdToolHelp::printValidateEepromHelp},
                {"--validateRedundantEeprom",
                 &VpdToolHelp::printValidateEepromHelp},
                {"--enterSplitMode", &VpdToolHelp::printSplitModeHelp},
                {"--exitSplitMode", &VpdToolHelp::printSplitModeHelp},
            };

        for (const auto& arg : args)
        {
            if (const auto it = flagToHelp.find(arg); it != flagToHelp.end())
            {
                it->second(this);
                return true;
            }
        }

        return false;
    }
};
} // namespace vpd
