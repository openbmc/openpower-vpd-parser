#include "config.h"

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

std::string SingleFab::getImFromPlanar() const noexcept
{
    try
    {
        types::BinaryVector l_imKeyword(KW_NAME_SIZE_IN_BYTES);
        types::BinaryVector l_imValue;
        uint8_t l_imKeywordValueSize = 0;
        std::fstream l_vpdFileStream;

        l_vpdFileStream.exceptions(
            std::ifstream::badbit | std::ifstream::failbit);

        l_vpdFileStream.open(SYSTEM_VPD_FILE_PATH,
                             std::ios::in | std::ios::binary);

        l_vpdFileStream.seekg(IM_KW_OFFSET, std::ios_base::beg);

        l_vpdFileStream.read(reinterpret_cast<char*>(&l_imKeyword[0]),
                             KW_NAME_SIZE_IN_BYTES);

        if (constants::kwdIM ==
            std::string(l_imKeyword.begin(), l_imKeyword.end()))
        {
            // Read keyword value size
            l_vpdFileStream.read(reinterpret_cast<char*>(&l_imKeywordValueSize),
                                 constants::VALUE_1);
            l_imValue.resize(static_cast<int>(l_imKeywordValueSize));

            // Read keyword value
            l_vpdFileStream.read(reinterpret_cast<char*>(&l_imValue[0]),
                                 l_imKeywordValueSize);
        }

        l_vpdFileStream.clear(std::ios_base::eofbit);

        std::ostringstream l_imData;
        for (const auto& l_byte : l_imValue)
        {
            l_imData << std::setw(2) << std::setfill('0') << std::hex
                     << static_cast<int>(l_byte);
        }
        return l_imData.str();
    }
    catch (const std::ifstream::failure& l_ex)
    {
        logging::logMessage(
            "Error while getting IM value from system planar EEPROM path: " +
            std::string(SYSTEM_VPD_FILE_PATH) +
            ", reason: " + std::string(l_ex.what()));
    }

    return std::string();
}
} // namespace vpd
