#include "config.h"

#include "single_fab.hpp"

#include "constants.hpp"
#include "event_logger.hpp"
#include "parser.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>
#include <utility/common_utility.hpp>
#include <utility/json_utility.hpp>
#include <utility/vpd_specific_utility.hpp>

namespace vpd
{
constexpr auto pimPersistVsbpPath =
    "/var/lib/phosphor-inventory-manager/xyz/openbmc_project/inventory/system/chassis/motherboard/com.ibm.ipzvpd.VSBP";
constexpr auto IM_SIZE_IN_BYTES = 0x04;
constexpr auto IM_KW_VALUE_OFFSET = 0x000005fb;

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
    {}

    return std::string();
}

std::string SingleFab::getImFromPlanar() const noexcept
{
    try
    {
        types::BinaryVector l_imValue(IM_SIZE_IN_BYTES);
        std::fstream l_vpdFileStream;

        l_vpdFileStream.exceptions(
            std::ifstream::badbit | std::ifstream::failbit);

        l_vpdFileStream.open(SYSTEM_VPD_FILE_PATH,
                             std::ios::in | std::ios::binary);

        l_vpdFileStream.seekg(IM_KW_VALUE_OFFSET, std::ios_base::beg);

        // Read keyword value
        l_vpdFileStream.read(reinterpret_cast<char*>(&l_imValue[0]),
                             IM_SIZE_IN_BYTES);

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
    {}

    return std::string();
}

bool SingleFab::setImOnPlanar(const std::string& i_imValue) const noexcept
{
    try
    {
        types::BinaryVector l_imValue;
        const std::string l_systemPlanarEepromPath = SYSTEM_VPD_FILE_PATH;

        // Convert string to vector of bytes
        for (auto l_value : i_imValue | std::views::chunk(2))
        {
            std::string l_byteString(l_value.begin(), l_value.end());
            l_imValue.push_back(
                static_cast<uint8_t>(std::stoi(l_byteString, nullptr, 16)));
        }

        std::shared_ptr<Parser> l_parserObj = std::make_shared<Parser>(
            l_systemPlanarEepromPath, nlohmann::json{});

        int l_bytes_updated = l_parserObj->updateVpdKeywordOnHardware(
            std::make_tuple(constants::recVSBP, constants::kwdIM, l_imValue));

        return l_bytes_updated > 0 ? true : false;
    }
    catch (const std::exception& l_ex)
    {
        return false;
    }
}

bool SingleFab::isFieldModeEnabled() const noexcept
{
    try
    {
        std::vector<std::string> l_cmdOutput =
            commonUtility::executeCmd("/sbin/fw_printenv -n fieldmode 2>&1");

        if (l_cmdOutput.size() > 0)
        {
            commonUtility::toLower(l_cmdOutput[0]);

            return l_cmdOutput[0] == "false" ? false : true;
        }
    }
    catch (const std::exception& l_ex)
    {}

    return false;
}

bool SingleFab::isValidImSeries(const std::string& l_imValue) const noexcept
{
    if (l_imValue.substr(0, 4) == POWER10_IM_SERIES ||
        l_imValue.substr(0, 4) == POWER11_IM_SERIES)
    {
        return true;
    }

    return false;
}

void SingleFab::singleFabImOverride()
{
    const std::string l_planarImValue = SingleFab::getImFromPlanar();
    const std::string l_eBmcImValue = SingleFab::getImFromPersistedLocation();
    const bool l_isFieldModeEnabled = SingleFab::isFieldModeEnabled();
    const bool l_isLabModeEnabled = !l_isFieldModeEnabled;
    const bool l_isPowerVsImage = vpdSpecificUtility::isPowerVsImage();
    const bool l_isNormalImage = !l_isPowerVsImage;

    if (!isValidImSeries(l_planarImValue))
    {
        // Create Errorlog for invalid IM series encountered
        EventLogger::createSyncPel(
            types::ErrorType::InvalidSystem, types::SeverityType::Error,
            __FILE__, __FUNCTION__, 0,
            std::string("Invalid IM found on system planar. ") +
                l_planarImValue,
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);

        return;
    }

    if (!l_eBmcImValue.empty())
    {
        if (isP10System(l_eBmcImValue))
        {
            if (isP10System(l_planarImValue))
            {
                if (l_isFieldModeEnabled && l_isNormalImage)
                {
                    // Create SRC1;
                    // Quiesce the BMC;
                }
            }
            else if (isP11System(l_planarImValue))
            {
                if (!(l_isLabModeEnabled && l_isNormalImage))
                {
                    // Create SRC1;
                    // Quiesce the BMC;
                }
            }
        }
        else if (isP11System(l_eBmcImValue))
        {
            if (l_isPowerVsImage)
            {
                // Create SRC1;
                // Quiesce the BMC;
            }
            else
            {
                if (isP10System(l_planarImValue))
                {
                    // update system IM Value to P11Series
                }
            }
        }
    }
    else
    {
        if (isP11System(l_planarImValue) && l_isPowerVsImage)
        {
            // Create SRC1;
            // Quiesce the BMC;
        }
        else if (isP10System(l_planarImValue) && l_isNormalImage)
        {
            if (l_isLabModeEnabled)
            {
                // create SRC0;
                // PE intervention required , PE to set IM based on Processor
                // used ; restart required;
            }
            else
            {
                // update system IM Value to P11Series;
            }
        }
    }
    return;
}
} // namespace vpd
