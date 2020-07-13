#include "config.h"

#include "types.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

class VpdTool
{
  private:
    const std::string fruPath;
    const std::string recordName;
    const std::string keyword;
    const std::string value;

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
     * @brief Adds FRU type and Location Code
     * Appends the type of the FRU and location code to the output
     *
     * @param[in] exIntf - extraInterfaces json from INVENTORY_JSON
     * @param[in] object - The D-Bus object to read the location code from
     * @param[out] kwVal - JSON object into which the FRU type and location code
     * are placed
     */
    void addFruTypeAndLocation(json exIntf, const std::string& object,
                               json& kwVal);

    /**
     * @brief Get VINI properties
     * Making a dbus call for properties [SN, PN, CC, FN, DR]
     * under VINI interface.
     *
     * @param[in] invPath - Value of inventory Path
     * @param[in] exIntf - extraInterfaces json from INVENTORY_JSON
     *
     * @return json output which gives the properties under invPath's VINI
     * interface
     */
    json getVINIProperties(std::string invPath, json exIntf);

    /**
     * @brief Get ExtraInterface Properties
     * Making a dbus call for those properties under extraInterfaces.
     *
     * @param[in] invPath - Value of inventory path
     * @param[in] extraInterface - One of the invPath's extraInterfaces whose
     * value is not null
     * @param[in] prop - All properties of the extraInterface.
     *
     * @return json output which gives the properties under invPath's
     *         extraInterface.
     */
    void getExtraInterfaceProperties(std::string invPath,
                                     std::string extraInterface, json prop,
                                     json exIntf, json& output);

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

    /** @brief printReturnCode */
    void printReturnCode(int returnCode);

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
     * in JSON format.
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
     * @brief ECC Fix
     * Fix the ecc for the given object by making a dbus call
     * to "FixBrokenEcc" method in VPD Manager interface.
     */
    void fixEcc();

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for dumpInventory option and
     * forceReset option.
     */
    VpdTool()
    {
    }

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for dumpObject option.
     */
    VpdTool(const std::string&& fru) : fruPath(std::move(fru))
    {
    }

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for readKeyword option.
     */
    VpdTool(const std::string&& fru, const std::string&& recName,
            const std::string&& kw) :
        fruPath(std::move(fru)),
        recordName(std::move(recName)), keyword(std::move(kw))
    {
    }

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
    {
    }

    /**
     * @brief Constructor
     * Constructor is called during object instantiation for fixEcc option.
     */
    VpdTool(const std::string&& fru, const std::string&& recName) :
        fruPath(std::move(fru)), recordName(std::move(recName))
    {
    }
};
