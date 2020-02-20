#include "vpd_tool_impl.hpp"

#define DUMP_INVENTORY
#define DUMP_OBJECT

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
 * @brief Call VINI
 *
 * Making a dbus call for those properties under VINI interface.
 * @param[in] invPath - Value of inventory Path
 *
 * @return json output which gives the properties under invPath's VINI interface
 */
json callVINI(string invPath)
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
json callExtraInterface(string invPath, string extraInterface, json prop)
{
    variant<string> response;
    json output = json::object({});
    json kwVal = json::object({});

    string objectName = INVENTORY_PATH + invPath;

    for (auto itProp : prop.items())
    {
        string kw = itProp.key();
        try
        {
            busctlCall(objectName, extraInterface, kw).read(response);

            if (auto str = get_if<string>(&response))
            {
                kwVal.emplace(kw, *str);
            }
        }
        catch (const SdBusError& e)
        {
            output.emplace(invPath, json::object({}));
        }
    }

    output.emplace(invPath, kwVal);
    return output;
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

    for (auto ex : itemEEPROM["extraInterfaces"].items())
    {
        if (!(ex.value().is_null()))
        {
            exIntfCheck = true;
            json j = callExtraInterface(itemEEPROM.at("inventoryPath"),
                                        ex.key(), ex.value());
            output.insert(j.begin(), j.end());
        }
    }

    if (!exIntfCheck)
    {
        if (itemEEPROM.find("inventoryPath") != itemEEPROM.end())
        {
            json j = callVINI(itemEEPROM.at("inventoryPath"));
            output.insert(j.begin(), j.end());
        }

        else
        {
            output.emplace(itemEEPROM.at("inventoryPath"), json::object({}));
        }
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
                        json j = interfaceDecider(itemEEPROM);
                        output.insert(j.begin(), j.end());
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
