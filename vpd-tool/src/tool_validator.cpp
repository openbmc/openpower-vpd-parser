#include "tool_validator.hpp"

#include "tool_constants.hpp"
#include "tool_error_codes.hpp"
#include "tool_utils.hpp"

#include <iostream>

namespace vpd
{
int Validator::checkCollectionStatus() const
{
    const auto l_vpdCollectionStatus = utils::readDbusProperty(
        constants::vpdManagerService, constants::vpdManagerObjectPath,
        constants::vpdCollectionInterface,
        constants::vpdCollectionStatusProperty);

    if (!l_vpdCollectionStatus.has_value())
    {
        std::cerr << "Failed to read VPD collection status property"
                  << std::endl;
        return static_cast<int>(l_vpdCollectionStatus.error());
    }

    if (!std::get_if<std::string>(&l_vpdCollectionStatus.value()))
    {
        std::cerr << "Received invalid type from DBus." << std::endl;
        return static_cast<int>(ErrorCode::DBUS_TYPE_MISMATCH);
    }

    if (*std::get_if<std::string>(&l_vpdCollectionStatus.value()) ==
        constants::vpdCollectionInProgress)
    {
        std::cout
            << "VPD collection is currently in progress, vpd-tool operations are blocked while the collection is in progress."
            << std::endl;
        return static_cast<int>(ErrorCode::NOT_ALLOWED);
    }

    return constants::SUCCESS;
}

int Validator::checkOptionValuePair(
    const CLI::Option* objectOption, const std::string& vpdPath,
    const CLI::Option* recordOption, const std::string& recordName,
    const CLI::Option* keywordOption, const std::string& keywordName,
    const CLI::Option* fileOption, const std::string& filePath,
    const CLI::Option* chassisOption, const std::optional<int>& chassisId) const
{
    if (!objectOption->empty() && vpdPath.empty())
    {
        std::cerr << "Given path is empty." << std::endl;
        return static_cast<int>(ErrorCode::INVALID_INPUT_PARAMETER);
    }

    if (!recordOption->empty() && recordName.empty())
    {
        std::cerr << "Record name is empty." << std::endl;
        return static_cast<int>(ErrorCode::RECORD_NOT_PROVIDED);
    }

    if (!keywordOption->empty() && keywordName.empty())
    {
        std::cerr << "keyword name is empty." << std::endl;
        return static_cast<int>(ErrorCode::KEYWORD_NAME_NOT_PROVIDED);
    }

    if (!fileOption->empty() && filePath.empty())
    {
        std::cerr << "File path is empty." << std::endl;
        return static_cast<int>(ErrorCode::EMPTY_FILE);
    }

    if (!chassisOption->empty() && !chassisId)
    {
        std::cerr << "Chassis id is empty." << std::endl;
        return static_cast<int>(ErrorCode::CHASSIS_ID_NOT_PROVIDED);
    }

    return constants::SUCCESS;
}

int Validator::validate(
    const CLI::Option* objectOption, const std::string& vpdPath,
    const CLI::Option* recordOption, const std::string& recordName,
    const CLI::Option* keywordOption, const std::string& keywordName,
    const CLI::Option* fileOption, const std::string& filePath,
    const CLI::Option* chassisOption, const std::optional<int>& chassisId) const
{
    if (auto rc = checkCollectionStatus(); rc != constants::SUCCESS)
    {
        return rc;
    }

    return checkOptionValuePair(objectOption, vpdPath, recordOption, recordName,
                                keywordOption, keywordName, fileOption,
                                filePath, chassisOption, chassisId);
}
} // namespace vpd
