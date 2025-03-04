#include "single_fab.hpp"

#include "constants.hpp"
#include "types.hpp"

#include <utility/json_utility.hpp>

namespace vpd
{
constexpr auto pimPersistVsbpPath =
    "/var/lib/phosphor-inventory-manager/xyz/openbmc_project/inventory/system/chassis/motherboard/com.ibm.ipzvpd.VSBP";
constexpr auto KW_NAME_SIZE_IN_BYTES = 0x02;
constexpr auto IM_KW_OFFSET = 0x000005f8;

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

void SingleFab::setIMOnPlanar(const std::string& i_imValue) const noexcept
{
    try
    {
        types::BinaryVector l_imValue;
        std::fstream l_vpdFileStream;
        types::BinaryVector l_imKwd(KW_NAME_SIZE_IN_BYTES);

        // Convert string to vector of bytes
        for (auto l_value : i_imValue | std::views::chunk(2))
        {
            std::string l_byteString(l_value.begin(), l_value.end());
            l_imValue.push_back(
                static_cast<uint8_t>(std::stoi(l_byteString, nullptr, 16)));
        }

        l_vpdFileStream.exceptions(
            std::fstream::failbit | std::fstream::badbit);
        l_vpdFileStream.open(SYSTEM_VPD_FILE_PATH,
                             std::ios::in | std::ios::out | std::ios::binary);

        l_vpdFileStream.seekp(IM_KW_OFFSET, std::ios::beg);

        l_vpdFileStream.read(reinterpret_cast<char*>(&l_imKwd[0]), 0x02);

        if (constants::kwdIM == std::string(l_imKwd.begin(), l_imKwd.end()))
        {
            // Skip length of the keyword value.
            l_vpdFileStream.seekp(0x01, std::ios::cur);
            l_vpdFileStream.write(
                reinterpret_cast<const char*>(l_imValue.data()),
                l_imValue.size());
        }

        l_vpdFileStream.close();
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to update IM value on planar, error : " +
                            std::string(l_ex.what()));
    }
}
} // namespace vpd
