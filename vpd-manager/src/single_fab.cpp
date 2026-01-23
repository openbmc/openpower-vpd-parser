#include "config.h"

#include "single_fab.hpp"

#include "constants.hpp"
#include "logger.hpp"
#include "parser.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>
#include <utility/common_utility.hpp>
#include <utility/event_logger_utility.hpp>
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
        uint16_t l_errCode = 0;
        auto l_parsedVsbpJsonObj =
            jsonUtility::getParsedJson(pimPersistVsbpPath, l_errCode);

        if (l_errCode)
        {
            throw JsonException(
                "Failed to parse JSON file [ " +
                    std::string(pimPersistVsbpPath) +
                    " ], error : " + commonUtility::getErrCodeMsg(l_errCode),
                pimPersistVsbpPath);
        }

        if (!l_parsedVsbpJsonObj.contains("value0") ||
            !l_parsedVsbpJsonObj["value0"].contains(constants::kwdIM) ||
            !l_parsedVsbpJsonObj["value0"][constants::kwdIM].is_array())
        {
            throw std::runtime_error("Mandatory tag(s) missing from JSON");
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
        const std::string l_systemPlanarPath(SYSTEM_VPD_FILE_PATH);
        Parser l_parserObj(l_systemPlanarPath, nlohmann::json{});

        std::shared_ptr<ParserInterface> l_vpdParserInstance =
            l_parserObj.getVpdParserInstance();

        auto l_readValue = l_vpdParserInstance->readKeywordFromHardware(
            std::make_tuple(constants::recVSBP, constants::kwdIM));

        if (auto l_keywordValue =
                std::get_if<types::BinaryVector>(&l_readValue);
            l_keywordValue && !l_keywordValue->empty())
        {
            std::ostringstream l_imData;
            for (const auto& l_byte : *l_keywordValue)
            {
                l_imData << std::setw(2) << std::setfill('0') << std::hex
                         << static_cast<int>(l_byte);
            }

            return l_imData.str();
        }
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

void SingleFab::updateSystemImValueInVpdToP11Series(
    std::string i_currentImValuePlanar) const noexcept
{
    bool l_retVal{false};
    if (!i_currentImValuePlanar.empty())
    {
        if (i_currentImValuePlanar.compare(
                constants::VALUE_4, constants::VALUE_1,
                std::to_string(constants::VALUE_3)) ==
            constants::STR_CMP_SUCCESS)
        {
            i_currentImValuePlanar.replace(constants::VALUE_4,
                                           constants::VALUE_1,
                                           std::to_string(constants::VALUE_2));
        }

        // update the IM value to P11 series(6000x). Replace the first character
        // of IM value string with '6'
        l_retVal = setImOnPlanar(i_currentImValuePlanar.replace(
            constants::VALUE_0, constants::VALUE_1,
            std::to_string(constants::VALUE_6)));
    }

    if (!l_retVal)
    {
        Logger::getLoggerInstance()->logMessage(
            std::string("Failed to update IM value to P11 series."),
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::InternalFailure,
                                types::SeverityType::Informational, 0,
                                std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt});
    }
}

int SingleFab::singleFabImOverride() const noexcept
{
    uint16_t l_errCode = 0;
    const std::string& l_planarImValue = getImFromPlanar();
    const std::string& l_eBmcImValue = getImFromPersistedLocation();
    const bool& l_isFieldModeEnabled =
        commonUtility::isFieldModeEnabled(l_errCode);

    if (l_errCode)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to check is field mode enabled, error : " +
            commonUtility::getErrCodeMsg(l_errCode));
    }

    const bool& l_isLabModeEnabled =
        !l_isFieldModeEnabled;                       // used for understanding
    const bool& l_isPowerVsImage = vpdSpecificUtility::isPowerVsImage();
    const bool& l_isNormalImage = !l_isPowerVsImage; // used for understanding

    if (!isValidImSeries(l_planarImValue))
    {
        // Create Errorlog for invalid IM series encountered
        Logger::getLoggerInstance()->logMessage(
            std::string("Invalid IM found on the system planar, IM value : ") +
                l_planarImValue,
            PlaceHolder::PEL,
            types::PelInfoTuple{types::ErrorType::InvalidSystem,
                                types::SeverityType::Error, 0, std::nullopt,
                                std::nullopt, std::nullopt, std::nullopt});

        return constants::SUCCESS;
    }

    if (!l_eBmcImValue.empty())
    {
        if (isP10System(l_eBmcImValue))
        {
            if (isP10System(l_planarImValue))
            {
                if (l_isFieldModeEnabled && l_isNormalImage)
                {
                    Logger::getLoggerInstance()->logMessage(
                        std::string("Mismatch in IM value found eBMC IM [") +
                            l_eBmcImValue + "] planar IM [" + l_planarImValue +
                            "] Field mode enabled [" +
                            ((l_isFieldModeEnabled) ? "true" : "false") + "]",
                        PlaceHolder::PEL,
                        types::PelInfoTuple{
                            types::ErrorType::SystemTypeMismatch,
                            types::SeverityType::Warning, 0, std::nullopt,
                            std::nullopt, std::nullopt, std::nullopt});

                    return constants::FAILURE;
                }
            }
            else if (isP11System(l_planarImValue))
            {
                if (!(l_isLabModeEnabled && l_isNormalImage))
                {
                    Logger::getLoggerInstance()->logMessage(
                        std::string("Mismatch in IM value found eBMC IM [") +
                            l_eBmcImValue + "] planar IM [" + l_planarImValue +
                            "] Field mode enabled [" +
                            ((l_isFieldModeEnabled) ? "true" : "false") + "]",
                        PlaceHolder::PEL,
                        types::PelInfoTuple{
                            types::ErrorType::SystemTypeMismatch,
                            types::SeverityType::Warning, 0, std::nullopt,
                            std::nullopt, std::nullopt, std::nullopt});

                    return constants::FAILURE;
                }
            }
        }
        else if (isP11System(l_eBmcImValue))
        {
            if (l_isPowerVsImage)
            {
                Logger::getLoggerInstance()->logMessage(
                    std::string("Mismatch in IM value found eBMC IM [") +
                        l_eBmcImValue + "] planar IM [" + l_planarImValue +
                        "] Field mode enabled [" +
                        ((l_isFieldModeEnabled) ? "true" : "false") + "]",
                    PlaceHolder::PEL,
                    types::PelInfoTuple{types::ErrorType::SystemTypeMismatch,
                                        types::SeverityType::Warning, 0,
                                        std::nullopt, std::nullopt,
                                        std::nullopt, std::nullopt});

                return constants::FAILURE;
            }
            else
            {
                if (isP10System(l_planarImValue))
                {
                    updateSystemImValueInVpdToP11Series(l_planarImValue);
                }
            }
        }
    }
    else
    {
        if (isP11System(l_planarImValue) && l_isPowerVsImage)
        {
            Logger::getLoggerInstance()->logMessage(
                std::string("Mismatch in IM value found eBMC IM [") +
                    l_eBmcImValue + "] planar IM [" + l_planarImValue +
                    "] Field mode enabled" +
                    ((l_isFieldModeEnabled) ? "true" : "false") + "]",
                PlaceHolder::PEL,
                types::PelInfoTuple{types::ErrorType::SystemTypeMismatch,
                                    types::SeverityType::Warning, 0,
                                    std::nullopt, std::nullopt, std::nullopt,
                                    std::nullopt});

            return constants::FAILURE;
        }
        else if (isP10System(l_planarImValue) && l_isNormalImage)
        {
            if (l_isLabModeEnabled)
            {
                Logger::getLoggerInstance()->logMessage(
                    std::string("Mismatch in IM value found eBMC IM [") +
                        l_eBmcImValue + "] planar IM [" + l_planarImValue +
                        "] Field mode enabled [" +
                        ((l_isFieldModeEnabled) ? "true" : "false") + "]",
                    PlaceHolder::PEL,
                    types::PelInfoTuple{types::ErrorType::UnknownSystemSettings,
                                        types::SeverityType::Warning, 0,
                                        std::nullopt, std::nullopt,
                                        std::nullopt, std::nullopt});
            }
            else
            {
                updateSystemImValueInVpdToP11Series(l_planarImValue);
            }
        }
    }
    return constants::SUCCESS;
}
} // namespace vpd
