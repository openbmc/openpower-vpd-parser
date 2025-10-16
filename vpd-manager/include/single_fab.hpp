#pragma once

#include "constants.hpp"
#include "event_logger.hpp"

#include <string>

namespace vpd
{
/**
 * @brief class to implement single fab feature.
 *
 * The class hosts functionalities required to support single FAB feature.
 *
 */
class SingleFab
{
  public:
    /**
     * @brief API to support single FAB feature.
     *
     * This API updates the IM value to the P11 series or creates PEL in invalid
     * case based on the IM value read from the cache and planar, considering
     * the system mode and image.
     *
     * System mode can be of field mode or lab mode and system image can be
     * special or normal image.
     *
     * @return 0 on success, -1 in case of failure.
     */
    int singleFabImOverride() const noexcept;

  private:
    /**
     * @brief API to get IM value from persisted location.
     *
     * @return IM value on success, empty string otherwise.
     */
    std::string getImFromPersistedLocation() const noexcept;

    /**
     * @brief API to get IM value from system planar EEPROM path.
     *
     * @return IM value on success, empty string otherwise.
     */
    std::string getImFromPlanar() const noexcept;

    /**
     * @brief API to update IM value on system planar EEPROM path.
     *
     * @param[in] i_imValue - IM value to be updated.
     *
     * @return true if value updated successfully, otherwise false.
     */
    bool setImOnPlanar(const std::string& i_imValue) const noexcept;

    /**
     * @brief API to update IM value on system planar EEPROM path to P11 series.
     *
     * @param[in] i_currentImValuePlanar - current IM value in planar EEPROM.
     */
    void updateSystemImValueInVpdToP11Series(
        std::string i_currentImValuePlanar) const noexcept;

    /**
     * @brief API to check if it is a P10 system.
     *
     * @param[in] i_imValue - IM value of the system.
     *
     * @return true, if P10 system. Otherwise false.
     */
    inline bool isP10System(const std::string& i_imValue) const noexcept
    {
        try
        {
            return !(i_imValue.compare(constants::VALUE_0, constants::VALUE_4,
                                       POWER10_IM_SERIES));
        }
        catch (const std::exception& l_ex)
        {
            EventLogger::createSyncPel(
                types::ErrorType::InternalFailure,
                types::SeverityType::Informational, __FILE__, __FUNCTION__, 0,
                std::string(
                    "Failed to check if system is of P10 series. Error : ") +
                    l_ex.what(),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
            return false;
        }
    }

    /**
     * @brief API to check if it is a P11 system.
     *
     * @param[in] i_imValue - IM value of the system.
     *
     * @return true, if P11 system. Otherwise false.
     */
    inline bool isP11System(const std::string& i_imValue) const noexcept
    {
        try
        {
            return !(i_imValue.compare(constants::VALUE_0, constants::VALUE_4,
                                       POWER11_IM_SERIES));
        }
        catch (const std::exception& l_ex)
        {
            EventLogger::createSyncPel(
                types::ErrorType::InternalFailure,
                types::SeverityType::Informational, __FILE__, __FUNCTION__, 0,
                std::string(
                    "Failed to check if system is of P11 series. Error : ") +
                    l_ex.what(),
                std::nullopt, std::nullopt, std::nullopt, std::nullopt);
            return false;
        }
    }

    /**
     * @brief API to check if it is a valid IM series.
     *
     * This API checks if the provided IM value is of either P10 or P11 series.
     *
     * @param[in] i_imValue - IM value of the system.
     *
     * @return true, if valid IM series. Otherwise false.
     */
    inline bool isValidImSeries(const std::string& l_imValue) const noexcept
    {
        return (isP10System(l_imValue) || isP11System(l_imValue));
    }

    // valid IM series.
    static constexpr auto POWER10_IM_SERIES = "5000";
    static constexpr auto POWER11_IM_SERIES = "6000";
};
} // namespace vpd
