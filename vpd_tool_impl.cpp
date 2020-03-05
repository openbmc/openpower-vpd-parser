#include "vpd_tool_impl.hpp"

#define DUMP_INVENTORY
#define DUMP_OBJECT
#define READ_KW

using sdbusplus::exception::SdBusError;

/**
 * @brief Debugger
 *
 * Displays the output in JSON.
 * @param[in] output - json output to be displayed
 */
void debugger(json output)
{
    cout << output.dump(4);
}

/**
 * @brief Binary to String Converter
 *
 * @param[in] b - Byte vector to be converted to string
 *
 * @return string form of the given byte vector
 */
string binaryToString(Binary b)
{
    ostringstream s;
    for (auto i : b)
    {
        s << static_cast<char>(i);
    }
    return s.str();
}

/**
 * @brief Busctl Call
 *
 * @param[in] objectName - dbus Object
 * @param[in] intreface - dbus Interface
 * @param[in] kw - keyword under the interface
 *
 * @return dbus call response
 */
auto busctlCall(string objectName, string interface, string kw)
{
    auto bus = sdbusplus::bus::new_default();
    auto properties = bus.new_method_call(
        "xyz.openbmc_project.Inventory.Manager", objectName.c_str(),
        "org.freedesktop.DBus.Properties", "Get");
    properties.append(interface);
    properties.append(kw);
    auto result = bus.call(properties);

    if (result.is_method_error())
    {
        throw runtime_error("Get api failed");
    }
    return result;
}

/**
 * @brief Adds FRU type and Location Code
 *
 * Appends the type of the FRU and location code to the output
 * @param[in] exIntf - extraInterfaces json from INVENTORY_JSON
 * @param[in] object - The D-Bus object to read the location code from
 * @param[out] kwVal - JSON object into which the FRU type and location code are
 *                     placed
 */
void addFruTypeAndLocation(json exIntf, const string& object, json& kwVal)
{
    for (auto i : exIntf.items())
    {
        if ((i.key().find("Item") != string::npos) && (i.value().is_null()))
        {
            kwVal.emplace("type", i.key());
            break;
        }
    }

    // Add location code.
    constexpr auto LOCATION_CODE_IF = "com.ibm.ipzvpd.Location";
    constexpr auto LOCATION_CODE_PROP = "LocationCode";

    try
    {
        variant<string> response;
        busctlCall(object, LOCATION_CODE_IF, LOCATION_CODE_PROP).read(response);

        if (auto prop = get_if<string>(&response))
        {
            kwVal.emplace(LOCATION_CODE_PROP, *prop);
        }
    }
    catch (const SdBusError& e)
    {
        kwVal.emplace(LOCATION_CODE_PROP, "");
    }
}

/**
 * @brief Call VINI
 *
 * Making a dbus call for those properties under VINI interface.
 * @param[in] invPath - Value of inventory Path
 * @param[in] exIntf - extraInterfaces json from INVENTORY_JSON
 *
 * @return json output which gives the properties under invPath's VINI interface
 */
json callVINI(string invPath, json exIntf)
{
    variant<Binary> response;
    json output = json::object({});
    json kwVal = json::object({});

    vector<string> keyword{"CC", "SN", "PN", "FN", "DR"};
    string interface = "com.ibm.ipzvpd.VINI";
    string objectName = INVENTORY_PATH + invPath;

    for (string kw : keyword)
    {
        try
        {
            busctlCall(objectName, interface, kw).read(response);

            if (auto vec = get_if<Binary>(&response))
            {
                kwVal.emplace(kw, binaryToString(*vec));
            }
        }
        catch (const SdBusError& e)
        {
            output.emplace(invPath, json::object({}));
        }
    }

    addFruTypeAndLocation(exIntf, objectName, kwVal);
    output.emplace(invPath, kwVal);
    return output;
}

/**
 * @brief Call ExtraInterface
 *
 * Making a dbus call for those properties under extraInterfaces.
 * @param[in] invPath - Value of inventory path
 * @param[in] extraInterface - One of the invPath's extraInterfaces whose value
 * is not null
 * @param[in] prop - All properties of the extraInterface.
 *
 * @return json output which gives the properties under invPath's
 * extraInterface.
 */
void callExtraInterface(string invPath, string extraInterface, json prop,
                        json exIntf, json& output)
{
    variant<string> response;
    string objectName = INVENTORY_PATH + invPath;

    for (auto itProp : prop.items())
    {
        string kw = itProp.key();
        try
        {
            busctlCall(objectName, extraInterface, kw).read(response);

            if (auto str = get_if<string>(&response))
            {
                output.emplace(kw, *str);
            }
        }
        catch (const SdBusError& e)
        {
            output.emplace(invPath, json::object({}));
        }
    }
    addFruTypeAndLocation(exIntf, objectName, output);
}

/**
 * @brief Interface Decider
 *
 * Decides whether to make the dbus call for
 * extraInterface or for VINI interface,
 * depending on the value of extraInterfaces object
 * in the inventory json.
 * @param[in] itemEEPROM - holds the reference of one of the EEPROM objects.
 *
 * @return json output for one of the EEPROM objects.
 */
json interfaceDecider(json& itemEEPROM)
{
    bool exIntfCheck = false;
    json output = json::object({});

    if (itemEEPROM.value("inherit", true))
    {
        if (itemEEPROM.find("inventoryPath") != itemEEPROM.end())
        {
            json j = callVINI(itemEEPROM.at("inventoryPath"),
                              itemEEPROM["extraInterfaces"]);
            output.insert(j.begin(), j.end());
        }

        else
        {
            output.emplace(itemEEPROM.at("inventoryPath"), json::object({}));
        }
    }
    else
    {
        json j;
        for (auto ex : itemEEPROM["extraInterfaces"].items())
        {
            if (!(ex.value().is_null()))
            {
                exIntfCheck = true;
                callExtraInterface(itemEEPROM.at("inventoryPath"), ex.key(),
                                   ex.value(), itemEEPROM["extraInterfaces"],
                                   j);
            }
        }
        output.emplace(itemEEPROM.at("inventoryPath"), j);
    }
    return output;
}

/**
 * @brief Parse Inventory JSON
 *
 * Parses the complete inventory json and depending upon
 * the user option makes the dbuscall for the frus
 * via interfaceDecider function.
 * @param[in] jsObject - Inventory json object
 * @param[in] flag - flag which tells about the user option(either dumpInventory
 * or dumpObject)
 * @param[in] fruPath - fruPath is empty for dumpInventory option and holds
 * valid fruPath for dumpObject option.
 *
 * @return output json
 */
json parseInvJson(json& jsObject, char flag, string fruPath)
{
    json output = json::object({});
    bool validObject = false;

    if (jsObject.find("frus") == jsObject.end())
    {
        throw runtime_error("Frus missing in Inventory json");
    }
    else
    {
        for (auto itemFRUS : jsObject["frus"].items())
        {
            for (auto itemEEPROM : itemFRUS.value())
            {
                try
                {
                    if (flag == 'O')
                    {
                        if (itemEEPROM.at("inventoryPath") == fruPath)
                        {
                            validObject = true;
                            json j = interfaceDecider(itemEEPROM);
                            output.insert(j.begin(), j.end());
                            return output;
                        }
                    }
                    else
                    {
                        if (itemEEPROM.value("inherit", true))
                        {
                            json j = callVINI(itemEEPROM.at("inventoryPath"),
                                              itemEEPROM["extraInterfaces"]);
                            output.insert(j.begin(), j.end());
                        }
                    }
                }
                catch (exception& e)
                {
                    cerr << e.what();
                }
            }
        }
        if ((flag == 'O') && (!validObject))
        {
            throw runtime_error(
                "Invalid object path. Refer --dumpInventory/-i option.");
        }
    }
    return output;
}

void VpdTool::dumpInventory(nlohmann::basic_json<>& jsObject)
{
    char flag = 'I';
    json output = parseInvJson(jsObject, flag, "");

#ifdef DUMP_INVENTORY
    debugger(output);
#endif
}

void VpdTool::dumpObject(nlohmann::basic_json<>& jsObject)
{
    char flag = 'O';
    json output = parseInvJson(jsObject, flag, fruPath);

#ifdef DUMP_OBJECT
    debugger(output);
#endif
}

void VpdTool::readKeyword()
{
    string interface = "com.ibm.ipzvpd.";
    variant<Binary> response;
    json output = json::object({});
    json kwVal = json::object({});

    busctlCall(INVENTORY_PATH + fruPath, interface + recordName, keyword)
        .read(response);

    if (auto vec = get_if<Binary>(&response))
    {
        kwVal.emplace(keyword, binaryToString(*vec));
    }

    output.emplace(fruPath, kwVal);

#ifdef READ_KW
    debugger(output);
#endif
}
