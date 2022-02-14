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
     * @brief Writes Memory mirror mode to BIOS
     *
     * Writes to the hb_memory_mirror_mode BIOS attribute, if the value is not
     * already the same as we are trying to write.
     *
     * @param[in] ammVal - The mirror mode as read from VPD.
     * @param[in] ammInBIOS - The mirror more in the BIOS table.
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
     * @brief Reads the hb_memory_mirror_mode attribute
     *
     * @return int64_t - The AMM BIOS attribute. -1 on failure.
     */
    std::string readBIOSAMM();

    /**
     * @brief Reads the hb_field_core_override attribute
     *
     * @return std::string - The FCO BIOS attribute. Empty string on failure.
     */
    int64_t readBIOSFCO();

    /**
     * @brief Restore BIOS attributes
     *
     * This function checks if we are coming out of a factory reset. If yes,
     * it checks the VPD cache for valid backed up copy of the FCO and AMM
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