#include "config.h"

#include "types.hpp"

#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

class VpdTool {
private:
  const string fruPath;
  const std::string recordName;
  const std::string keyword;

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
  auto makeDBusCall(const string &objectName, const string &interface,
                    const string &kw);

  /**
   * @brief Adds FRU type and Location Code
   * Appends the type of the FRU and location code to the output
   *
   * @param[in] exIntf - extraInterfaces json from INVENTORY_JSON
   * @param[in] object - The D-Bus object to read the location code from
   * @param[out] kwVal - JSON object into which the FRU type and location code
   * are placed
   */
  void addFruTypeAndLocation(json exIntf, const string &object, json &kwVal);

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
  json getVINIProperties(string invPath, json exIntf);

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
  void getExtraInterfaceProperties(string invPath, string extraInterface,
                                   json prop, json exIntf, json &output);

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
  json interfaceDecider(json &itemEEPROM);

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
  json parseInvJson(const json &jsObject, char flag, string fruPath);

public:
  /**
   * @brief Dump the complete inventory in JSON format
   *
   * @param[in] jsObject - Inventory JSON specified in configure file.
   */
  void dumpInventory(const nlohmann::basic_json<> &jsObject);

  /**
   * @brief Dump the given inventory object in JSON format
   *
   * @param[in] jsObject - Inventory JSON specified in configure file.
   */
  void dumpObject(const nlohmann::basic_json<> &jsObject);

  /**
   * @brief Read keyword
   * Read the given object path, record name and keyword
   * from the inventory and display the value of the keyword
   * in JSON format.
   */
  void readKeyword();

  /**
   * @brief A Constructor
   * Constructor is called during the
   * object instantiation for dumpInventory option.
   */
  VpdTool() {}

  /**
   * @brief Constructor
   * Constructor is called during the
   * object instantiation for dumpObject option.
   */
  VpdTool(const string &&fru) : fruPath(move(fru)) {}

  /**
   * @brief Constructor
   * Constructor is called during the
   * object instantiation for readKeyword option.
   */
  VpdTool(const std::string &&fru, const std::string &&recName,
          const std::string &&kw)
      : fruPath(std::move(fru)), recordName(std::move(recName)),
        keyword(std::move(kw)) {}
};
