#pragma once

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
  public:
    /**
     * @brief Print operation-specific help for the given operation flag.
     *
     * Scans argv for an operation flag (e.g. -w, -r, -i, -e,
     * --enterSplitMode, --exitSplitMode) combined with --help / -h and
     * dispatches to the corresponding private print method. If no operation
     * flag is found alongside --help, the method prints generic help and
     * returns true.
     *
     * @param[in] argc - Argument count (same as main's argc).
     * @param[in] argv - Argument vector (same as main's argv).
     *
     * @return true if help was called, false otherwise.
     */
    bool printHelp(int argc, char** argv) const;

  private:
    /** @brief Print help text for the writeKeyword operation. */
    void printWriteKeywordHelp() const noexcept;

    /** @brief Print general help for supported vpd-tool operations. */
    void printGenericHelp() const noexcept;

    /** @brief Print help for the readKeyword operation. */
    void printReadKeywordHelp() const noexcept;

    void printDumpInventoryHelp() const noexcept;
};
} // namespace vpd
