#include "config.h"

#include "editor_impl.hpp"
#include "ibm_vpd_utils.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>

#include <string>

using json = nlohmann::json;

// <S.no, Record, Keyword, D-Bus value, HW value, Data mismatch>
using SystemCriticalData =
    std::vector<std::tuple<uint8_t, std::string, std::string, std::string,
                           std::string, std::string>>;

class VpdTool
{
  private:
    const std::string fruPath;
    const std::string recordName;
    const std::string keyword;
    const std::string value;
    bool objFound = true;
    SystemCriticalData recKwData;

    // Store Type of FRU
    std::string fruType;

    /**
     * @brief Debugger
     * Displays the output in JSON.
     *
     * @param[in] output - json output to be displayed
     */
    void debugger(json output);

    /**
     * @brief make Dbus Call
     *
     * @param[in] objectName - dbus Object
     * @param[in] interface - dbus Interface
     * @param[in] kw - keyword under the interface
     *
     * @return dbus call response
     */
    auto makeDBusCall(const std::string& objectName,
                      const std::string& interface, const std::string& kw);

    /**
     * @brief Get VINI properties
     * Making a dbus call for properties [SN, PN, CC, FN, DR]
     * under VINI interface.
     *
     * @param[in] invPath - Value of inventory Path
     *
     * @return json output which gives the properties under invPath's VINI
     * interface
     */
    json getVINIProperties(std::string invPath);

    /**
     * @brief Get ExtraInterface Properties
     * Making a dbus call for those properties under extraInterfaces.
     *
     * @param[in] invPath - Value of inventory path
     * @param[in] extraInterface - One of the invPath's extraInterfaces whose
     * value is not null
     * @param[in] prop - All properties of the extraInterface.
     * @param[out] output - output json which has the properties under invPath's
     * extra interface.
     */
    void getExtraInterfaceProperties(const std::string& invPath,
                                     const std::string& extraInterface,
                                     const json& prop, json& output);

    /**
     * @brief Interface Decider
     * Decides whether to make the dbus call for
     * getting properites from extraInterface or from
     * VINI interface, depending on the value of
     * extraInterfaces object in the inventory json.
     *
     * @param[in] itemEEPROM - holds the reference of one of the EEPROM objects.
     *
     * @return json output for one of the EEPROM objects.
     */
    json interfaceDecider(json& itemEEPROM);

    /**
     * @brief Parse Inventory JSON
     * Parses the complete inventory json and depending upon
     * the user option makes the dbuscall for the frus
     * via interfaceDecider function.
     *
     * @param[in] jsObject - Inventory json object
     * @param[in] flag - flag which tells about the user option(either
     * dumpInventory or dumpObject)
     * @param[in] fruPath - fruPath is empty for dumpInventory option and holds
     *                      valid fruPath for dumpObject option.
     *
     * @return output json
     */
    json parseInvJson(const json& jsObject, char flag, std::string fruPath);

    /**
     * @brief eraseInventoryPath
     * Remove the INVENTORY_PATH - "/xyz/openbmc_project/inventory"
     * for code convenience.
     *
     * @param[out] fru - Reference to the fru path whose INVENTORY_PATH is
     * stripped off.
     */
    void eraseInventoryPath(std::string& fru);

    /**
     * @brief printReturnCode
     * Prints the return code of the program in console output, whenever
     * the program fails to exit successfully.
     *
     * @param[in] returnCode - return code of the program.
     */
    void printReturnCode(int returnCode);

    /**
     * @brief Convert hex/ascii values to Binary
     * @param[in] - value in hex/ascii.
     * @param[out] - value in binary.
     */
    openpower::vpd::Binary toBinary(const std::string& value);

    /**
     * @brief Get the json which has Present property value of the given fru.
     * @param[in] invPath - inventory path of the fru.
     * @return output json which has the Present property value.
     */
    json getPresentPropJson(const std::string& invPath);

    /**
     * @brief Parse through the options to fix system VPD
     *
     * @param[in] json - Inventory JSON
     * @param[in] backupEEPROMPath - Backup VPD path
     */
    void parseSVPDOptions(const nlohmann::json& json,
                          const std::string& backupEEPROMPath);

    /**
     * @brief List of user options that can be used to fix system VPD keywords.
     */
    enum UserOption
    {
        EXIT = 0,
        BACKUP_DATA_FOR_ALL = 1,
        SYSTEM_BACKPLANE_DATA_FOR_ALL = 2,
        MORE_OPTIONS = 3,
        BACKUP_DATA_FOR_CURRENT = 4,
        SYSTEM_BACKPLANE_DATA_FOR_CURRENT = 5,
        NEW_VALUE_ON_BOTH = 6,
        SKIP_CURRENT = 7
    };

    /**
     * @brief Print options to fix system VPD.
     * @param[in] option - Option to use.
     */
    void printFixSystemVPDOption(UserOption option);

    /**
     * @brief Get System VPD data stored in cache
     *
     * @param[in] svpdBusData - Map of system VPD record data.
     */
    void getSystemDataFromCache(
        openpower::vpd::inventory::IntfPropMap& svpdBusData);

    /**
     * @brief Get data from file and store in binary format
     *
     * @param[out] data - The resulting binary data
     *
     * @return If operation is success return true, else on failure return
     * false.
     */
    bool fileToVector(openpower::vpd::Binary& data);

    /**
     * @brief Copy string data to file.
     *
     * @param[in] input - input string
     *
     * @return If operation is success return true, else on failure return
     * false.
     */
    bool copyStringToFile(const std::string& input);

  public:
    /**
     * @brief Dump the complete inventory in JSON format
     *
     * @param[in] jsObject - Inventory JSON specified in configure file.
     */
    void dumpInventory(const nlohmann::basic_json<>& jsObject);

    /**
     * @brief Dump the given inventory object in JSON format
     *
     * @param[in] jsObject - Inventory JSON specified in configure file.
     */
    void dumpObject(const nlohmann::basic_json<>& jsObject);

    /**
     * @brief Read keyword
     * Read the given object path, record name and keyword
     * from the inventory and display the value of the keyword
     * in JSON format. The read value will be piped to file if --file is given
     * by the user. Else the value read will be displayed on console.
     */
    void readKeyword();

    /**
     * @brief Update Keyword
     * Update the given keyword with the given value.
     *
     * @return return code (Success(0)/Failure(-1))
     */
    int updateKeyword();

    /**
     * @brief Force Reset
     * Clearing the inventory cache data and restarting the
     * phosphor inventory manager and also retriggering all the
     * udev events.
     *
     * @param[in] jsObject - Inventory JSON specified in configure file.
     */
    void forceReset(const nlohmann::basic_json<>& jsObject);

    /**
     * @brief Update Hardware
     * The given data is updated only on the given hardware path and not on dbus
     * for the given record-keyword pair. The user can now update record-keyword
     * value for any hardware path irrespective of whether its present or not in
     * VPD JSON, by providing a valid offset. By default offset takes 0.
     *
     * @param[in] offset - VPD offset.
     * @return returncode (success/failure).
     */
    int updateHardware(const uint32_t offset);

    /**
     * @brief Read Keyword from Hardware
     * This api is to read a keyword directly from the hardware. The hardware
     * path, record name and keyword name are received at the time of
     * initialising the constructor.
     * The user can now read keyword from any hardware path irrespective of
     * whether its present or not in VPD JSON, by providing a valid offset. By
     * default offset takes 0. The read value can be either saved in a
     * file/displayed on console.
     *
     * @param[in] startOffset - VPD offset.
     */
    void readKwFromHw(const uint32_t& startOffset);

    /**
     * @brief Fix System VPD keyword.
     * This API provides an interactive way to fix system VPD keywords that are
     * part of restorable record-keyword pairs. The user can use this option to
     * restore the restorable keywords in cache or in hardware or in both cache
     * and hardware.
     * @return returncode (success/failure).
     */
    int fixSystemVPD();

    /**
     * @brief Clean specific keywords in system backplane VPD
     *
     * @return return code (success/failure)
     */
    int cleanSystemVPD();

    /**
     * @brief Fix system VPD and its backup VPD
     * API is triggerred if the backup of system VPD has to be taken in a
     * hardware VPD. User can use the --fixSystemVPD option to restore the
     * keywords in backup VPD and/or system VPD.
     *
     * @param[in] backupEepromPath - Backup VPD path
     * @param[in] backupInvPath - Backup inventory path
     * @return returncode
     */
    int fixSystemBackupVPD(const std::string& backupEepromPath,
                           const std::string& backupInvPath);

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for dumpInventory option and
     * forceReset option.
     */
    VpdTool() {}

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for dumpObject option.
     */
    VpdTool(const std::string&& fru) : fruPath(std::move(fru)) {}

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for updateKeyword option.
     */

    VpdTool(const std::string&& fru, const std::string&& recName,
            const std::string&& kw, const std::string&& val) :
        fruPath(std::move(fru)),
        recordName(std::move(recName)), keyword(std::move(kw)),
        value(std::move(val))
    {}
};
