#pragma once

#include <string>

namespace vpd
{

// valid IM series.
constexpr auto POWER10_IM_SERIES = "5000";
constexpr auto POWER11_IM_SERIES = "6000";
/**
 * @brief class to implement single fab feature.
 *
 * The class hosts functionalities required to support single FAB feature.
 *
 */
class SingleFab
{
  public:
    /** @brief API to support single FAB feature.
     *
     * This API updates the IM value to the P11 series or creates PEL based on
     * the IM value read from the cache and planar, considering the system mode
     * and type.
     *
     * System mode can be of field mode or lab mode and system type can be
     * powervs or normal system.
     */
    void singleFabImOverride();

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
     * @brief API to check is field mode enabled.
     *
     * @return true, if field mode is enabled. otherwise false.
     */
    bool isFieldModeEnabled() const noexcept;

    /**
     * @brief API to check if it is a P10 system
     *
     * @param[in] i_imValue - IM value of the system.
     *
     * @return true, if P10 system. Otherwise false.
     */
    inline bool isP10System(const std::string& i_imValue)
    {
        return (i_imValue.substr(0, 4) == POWER10_IM_SERIES) ? true : false;
    }

    /**
     * @brief API to check if it is a P11 system
     *
     * @param[in] i_imValue - IM value of the system.
     *
     * @return true, if P11 system. Otherwise false.
     */
    inline bool isP11System(const std::string& i_imValue)
    {
        return (i_imValue.substr(0, 4) == POWER11_IM_SERIES) ? true : false;
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
    bool isValidImSeries(const std::string& i_imValue) const noexcept;
};
} // namespace vpd
