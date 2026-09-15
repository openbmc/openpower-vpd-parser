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
  private:
    /** @brief Print help for the writeKeyword operation. */
    void printWriteKeywordHelp() const noexcept;

    /** @brief Print general help for supported vpd-tool operations. */
    void printGenericHelp() const noexcept;

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
    bool printHelp(int argc, char** argv) const;
};
} // namespace vpd
