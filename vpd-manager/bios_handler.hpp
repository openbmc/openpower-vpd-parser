#pragma once

#include "types.hpp"

#include <stdint.h>

#include <sdbusplus/asio/connection.hpp>
#include <string>

namespace openpower
{
namespace vpd
{
namespace manager
{

class Manager;
/**
 * @brief A class that handles changes to BIOS attributes backed by VPD.
 *
 * This class has APIs that handle updates to BIOS attributes that need to
 * be backed up to VPD. It mainly does the following:
 * 1) Checks if the VPD keywords that BIOS attributes are backed to are
 * uninitialized. If so, it initializes them.
 * 2) Listens for changes to BIOS attributes and synchronizes them to the
 * appropriate VPD keyword.
 *
 * Since on a factory reset like scenario, the BIOS attributes are initialized
 * by PLDM, this code waits until PLDM has grabbed a bus name before attempting
 * any syncs.
 */
class BiosHandler
{
  public:
    // Some default and deleted constructors and assignments.
    BiosHandler() = delete;
    BiosHandler(const BiosHandler&) = delete;
    BiosHandler& operator=(const BiosHandler&) = delete;
    BiosHandler(Manager&&) = delete;
    BiosHandler& operator=(BiosHandler&&) = delete;
    ~BiosHandler() = default;

    BiosHandler(std::shared_ptr<sdbusplus::asio::connection>& conn,
                Manager& manager) :
        conn(conn),
        manager(manager)
    {
        checkAndListenPLDMService();
    }

  private:
    /**
     * @brief Check if PLDM service is running and run BIOS sync
     *
     * This API checks if the PLDM service is running and if yes it will start
     * an immediate sync of BIOS attributes. If the service is not running, it
     * registers a listener to be notified when the service starts so that a
     * restore can be performed.
     */
    void checkAndListenPLDMService();

    /**
     * @brief Register listener for changes to BIOS Attributes.
     *
     * The VPD manager needs to listen to changes to certain BIOS attributes
     * that are backed by VPD. When the attributes we are interested in
     * change, the VPD manager will make sure that we write them back to the
     * VPD keywords that back them up.
     */
    void listenBiosAttribs();

    /**
     * @brief Callback for BIOS Attribute changes
     *
     * Checks if the BIOS attribute(s) changed are those backed up by VPD. If
     * yes, it will update the VPD with the new attribute value.
     * @param[in] msg - The callback message.
     */
    void biosAttribsCallback(sdbusplus::message_t& msg);

    /**
     * @brief Persistently saves the Memory mirror mode
     *
     * Memory mirror mode setting is saved to the UTIL/D0 keyword in the
     * motherboard VPD. If the mirror mode in BIOS is "Disabled", set D0 to 1,
     * if "Enabled" set D0 to 2
     *
     * @param[in] mirrorMode - The mirror mode BIOS attribute.
     */
    void saveAMMToVPD(const std::string& mirrorMode);

    /**
     * @brief Persistently saves the Field Core Override setting
     *
     * Saves the field core override value (FCO) into the VSYS/RG keyword in
     * the motherboard VPD.
     *
     * @param[in] fcoVal - The FCO value as an integer.
     */
    void saveFCOToVPD(int64_t fcoVal);

    /**
     * @brief Persistently saves the Keep and Clear setting
     *
     * Keep and clear setting is saved to the UTIL/D1 keyword's 0th bit in the
     * motherboard VPD. If the keep and clear in BIOS is "Disabled", set D1:0 to
     * 0, if "Enabled" set D1:0 to 1
     *
     * @param[in] keepAndClear - The keep and clear BIOS attribute.
     */
    void saveKeepAndClearToVPD(const std::string& keepAndClear);

    /**
     * @brief Persistently saves the Create default LPAR setting
     *
     * Create default LPAR setting is saved to the UTIL/D1 keyword's 1st bit in
     * the motherboard VPD. If the create default LPAR in BIOS is "Disabled",
     * set D1:1 to 0, if "Enabled" set D1:1 to 1
     *
     * @param[in] createDefaultLpar - The mirror mode BIOS attribute.
     */
    void saveCreateDefaultLparToVPD(const std::string& createDefaultLpar);

    /**
     * @brief Persistently saves the Clear NVRAM setting
     *
     * Create default LPAR setting is saved to the UTIL/D1 keyword's 2nd bit in
     * the motherboard VPD. If the clear NVRAM in BIOS is "Disabled",
     * set D1:2 to 0, if "Enabled" set D1:2 to 1
     *
     * @param[in] createDefaultLpar - The mirror mode BIOS attribute.
     */
    void saveClearNVRAMToVPD(const std::string& clearNVRAM);

    /**
     * @brief Writes Memory mirror mode to BIOS
     *
     * Writes to the hb_memory_mirror_mode BIOS attribute, if the value is
     * not already the same as we are trying to write.
     *
     * @param[in] ammVal - The mirror mode as read from VPD.
     * @param[in] ammInBIOS - The mirror mode in the BIOS table.
     */
    void saveAMMToBIOS(const std::string& ammVal, const std::string& ammInBIOS);

    /**
     * @brief Writes Field Core Override to BIOS
     *
     * Writes to the hb_field_core_override BIOS attribute, if the value is not
     * already the same as we are trying to write.
     *
     * @param[in] fcoVal - The FCO value as read from VPD.
     * @param[in] fcoInBIOS - The FCO value already in the BIOS table.
     */
    void saveFCOToBIOS(const std::string& fcoVal, int64_t fcoInBIOS);

    /**
     * @brief Writes Keep and clear setting to BIOS
     *
     * Writes to the pvm_keep_and_clear BIOS attribute, if the value is
     * not already the same as we are trying to write.
     *
     * @param[in] keepAndClear - The keep and clear as read from VPD.
     * @param[in] keepAndClearInBIOS - The keep and clear in the BIOS table.
     */
    void saveKeepAndClearToBIOS(const std::string& keepAndClear,
                                const std::string& keepAndClearInBIOS);

    /**
     * @brief Writes Create default LPAR setting to BIOS
     *
     * Writes to the pvm_create_default_lpar BIOS attribute, if the value is
     * not already the same as we are trying to write.
     *
     * @param[in] createDefaultLpar - The create default LPAR as read from VPD.
     * @param[in] createDefaultLparInBIOS - The create default LPAR in the BIOS
     * table.
     */
    void
        saveCreateDefaultLparToBIOS(const std::string& createDefaultLpar,
                                    const std::string& createDefaultLparInBIOS);

    /**
     * @brief Writes Clear NVRAM setting to BIOS
     *
     * Writes to the pvm_clear_nvram BIOS attribute, if the value is
     * not already the same as we are trying to write.
     *
     * @param[in] clearNVRAM - The clear NVRAM as read from VPD.
     * @param[in] clearNVRAMInBIOS - The clear NVRAM in the BIOS table.
     */
    void saveClearNVRAMToBIOS(const std::string& clearNVRAM,
                              const std::string& clearNVRAMInBIOS);

    /**
     * @brief Reads the hb_memory_mirror_mode attribute
     *
     * @return std::string - The AMM BIOS attribute. Empty string on failure.
     */
    std::string readBIOSAMM();

    /**
     * @brief Reads the hb_field_core_override attribute
     *
     * @return int64_t - The FCO BIOS attribute.  -1 on failure.
     */
    int64_t readBIOSFCO();

    /**
     * @brief Reads the pvm_keep_and_clear attribute
     *
     * @return std::string - The Keep and clear BIOS attribute. Empty string on
     * failure.
     */
    std::string readBIOSKeepAndClear();

    /**
     * @brief Reads the pvm_create_default_lpar attribute
     *
     * @return std::string - The Create default LPAR BIOS attribute. Empty
     * string on failure.
     */
    std::string readBIOSCreateDefaultLpar();

    /**
     * @brief Reads the pvm_clear_nvram attribute
     *
     * @return std::string - The Clear NVRAM BIOS attribute. Empty
     * string on failure.
     */
    std::string readBIOSClearNVRAM();

    /**
     * @brief Restore BIOS attributes
     *
     * This function checks if we are coming out of a factory reset. If yes,
     * it checks the VPD cache for valid backed up copy of the applicable
     * BIOS attributes. If valid values are found in the VPD, it will apply
     * those to the BIOS attributes.
     */
    void restoreBIOSAttribs();

    /**
     * @brief Reference to the connection.
     */
    std::shared_ptr<sdbusplus::asio::connection>& conn;

    /**
     * @brief Reference to the manager.
     */
    Manager& manager;
}; // class BiosHandler
} // namespace manager
} // namespace vpd
} // namespace openpower