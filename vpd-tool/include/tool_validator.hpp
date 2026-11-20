#pragma once

#include <CLI/CLI.hpp>

#include <optional>
#include <string>

namespace vpd
{

/**
 * @brief Centralized validator for all VPD-tool operations.
 *
 * This class is responsible for determining whether a requested VPD operation
 * is permitted to run by performing the necessary pre-operation validations.
 * These validations include common checks applicable to all operations as well
 * as operation-specific checks.
 */
class Validator
{
  public:
    /**
     * @brief Performs common validation checks
     *
     * This API serves as the entry point for common pre-operation validations.
     * It checks whether VPD collection is in progress and validates the
     * command-line option-value pairs.
     *
     * @param[in] objectOption - Option to pass object path.
     * @param[in] vpdPath - VPD path.
     * @param[in] recordOption - Option to pass record name.
     * @param[in] recordName - Record name.
     * @param[in] keywordOption - Option to pass keyword name.
     * @param[in] keywordName - Keyword name.
     * @param[in] fileOption - Option to pass file path.
     * @param[in] filePath - File path.
     * @param[in] chassisOption - Option to pass chassis ID.
     * @param[in] chassisId - Chassis ID.
     *
     * @return SUCCESS if all common validation checks pass; otherwise, returns
     * the appropriate error code.
     */
    int validate(const CLI::Option* objectOption, const std::string& vpdPath,
                 const CLI::Option* recordOption, const std::string& recordName,
                 const CLI::Option* keywordOption,
                 const std::string& keywordName, const CLI::Option* fileOption,
                 const std::string& filePath, const CLI::Option* chassisOption,
                 const std::optional<int>& chassisId) const;

  private:
    /**
     * @brief Checks if the VPD collection is currently in progress.
     *
     * If the collection is in progress, any subsequent VPD operations are
     * blocked.
     *
     * @return SUCCESS if the collection is not in progress, otherwise
     * appropriate error code.
     */
    int checkCollectionStatus() const;

    /**
     * @brief Validates that command line options have valid values.
     *
     * Checks if the required arguments (like record/keyword sizes, non-empty
     * paths) are correct based on the options specified.
     *
     * @param[in] objectOption - Option to pass object path.
     * @param[in] vpdPath - VPD path.
     * @param[in] recordOption - Option to pass record name.
     * @param[in] recordName - Record name.
     * @param[in] keywordOption - Option to pass keyword name.
     * @param[in] keywordName - Keyword name.
     * @param[in] fileOption - Option to pass file path.
     * @param[in] filePath - File path.
     * @param[in] chassisOption - Option to pass chassis id.
     * @param[in] chassisId - Chassis id.
     *
     * @return SUCCESS if option-value pairs are valid, otherwise appropriate
     * error code.
     */
    int checkOptionValuePair(
        const CLI::Option* objectOption, const std::string& vpdPath,
        const CLI::Option* recordOption, const std::string& recordName,
        const CLI::Option* keywordOption, const std::string& keywordName,
        const CLI::Option* fileOption, const std::string& filePath,
        const CLI::Option* chassisOption,
        const std::optional<int>& chassisId) const;
};
} // namespace vpd
