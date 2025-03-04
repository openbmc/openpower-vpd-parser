#include "config.h"

#include "single_fab.hpp"

#include "constants.hpp"
#include "parser.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>
#include <utility/json_utility.hpp>

namespace vpd
{
constexpr auto pimPersistVsbpPath =
    "/var/lib/phosphor-inventory-manager/xyz/openbmc_project/inventory/system/chassis/motherboard/com.ibm.ipzvpd.VSBP";

std::string SingleFab::getImFromPersistedLocation() const noexcept
{
    try
    {
        auto l_parsedVsbpJsonObj =
            jsonUtility::getParsedJson(pimPersistVsbpPath);
        if (!l_parsedVsbpJsonObj.contains("value0") ||
            !l_parsedVsbpJsonObj["value0"].contains(constants::kwdIM) ||
            !l_parsedVsbpJsonObj["value0"][constants::kwdIM].is_array())
        {
            throw std::runtime_error(
                "Json is empty or mandatory tag(s) missing from JSON");
        }

        const types::BinaryVector l_imValue =
            l_parsedVsbpJsonObj["value0"][constants::kwdIM]
                .get<types::BinaryVector>();

        std::ostringstream l_imData;
        for (const auto& l_byte : l_imValue)
        {
            l_imData << std::setw(2) << std::setfill('0') << std::hex
                     << static_cast<int>(l_byte);
        }
        return l_imData.str();
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Error while getting IM value from PIM persisted file: " +
            std::string(pimPersistVsbpPath) +
            ", reason: " + std::string(l_ex.what()));
    }

    return std::string();
}

bool SingleFab::setImOnPlanar(const std::string& i_imValue) const noexcept
{
    try
    {
        nlohmann::json l_json;
        types::BinaryVector l_imValue;

        // Convert string to vector of bytes
        for (auto l_value : i_imValue | std::views::chunk(2))
        {
            std::string l_byteString(l_value.begin(), l_value.end());
            l_imValue.push_back(
                static_cast<uint8_t>(std::stoi(l_byteString, nullptr, 16)));
        }

        std::shared_ptr<Parser> l_parserObj =
            std::make_shared<Parser>(SYSTEM_VPD_FILE_PATH, l_json);
        int l_bytes_updated = l_parserObj->updateVpdKeywordOnHardware(
            std::make_tuple(constants::recVSBP, constants::kwdIM, l_imValue));

        return l_bytes_updated > 0 ? true : false;
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to update IM value on planar, error : " +
                            std::string(l_ex.what()));
        return false;
    }
}
} // namespace vpd
