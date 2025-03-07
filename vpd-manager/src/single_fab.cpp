#include "single_fab.hpp"

#include "constants.hpp"
#include "types.hpp"

#include <utility/json_utility.hpp>

namespace vpd
{
constexpr auto pimPersistVsbpPath =
    "/var/lib/phosphor-inventory-manager/xyz/openbmc_project/inventory/system/chassis/motherboard/com.ibm.ipzvpd.VSBP";

std::string SingleFab::getImFromPersistedLocation() const noexcept
{
    std::string l_imFilePath{pimPersistVsbpPath};

    try
    {
        auto l_parsedVsbpJsonObj = jsonUtility::getParsedJson(l_imFilePath);
        if (!l_parsedVsbpJsonObj.contains("value0") ||
            !l_parsedVsbpJsonObj["value0"].contains(constants::kwdIM) ||
            !l_parsedVsbpJsonObj["value0"][constants::kwdIM].is_array())
        {
            throw std::runtime_error(
                "Json is empty or mandatory tag(s) missing from JSON");
        }

        types::BinaryVector l_imValue =
            l_parsedVsbpJsonObj["value0"][constants::kwdIM]
                .get<types::BinaryVector>();

        std::ostringstream l_imData;
        for (auto& l_byte : l_imValue)
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
            l_imFilePath + ", reason: " + std::string(l_ex.what()));
    }

    return std::string();
}
} // namespace vpd
