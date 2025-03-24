#pragma once

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
    // ToDo: public API to be implemented, which can be called by the user to
    // support single FAB.

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
     * This API updates the IM value in VPD to P11 series (6000x).
     *
     * @param[in] i_currentImValuePlanar - current IM value in planar EEPROM.
     *
     * @return true if IM value is updated successfully, otherwise false.
     *
     */
    bool updateSystemImValueInVpdToP11Series(
        const std::string& i_currentImValuePlanar) const noexcept;
};
} // namespace vpd
