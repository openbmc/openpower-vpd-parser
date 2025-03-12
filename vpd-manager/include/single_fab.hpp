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
};
} // namespace vpd
