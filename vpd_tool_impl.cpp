#include "vpd_tool_impl.hpp"

#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sdbusplus/bus.hpp>
#include <sstream>
#include <variant>
#include <vector>

using namespace std;
using json = nlohmann::json;
using sdbusplus::exception::SdBusError;
using namespace openpower::vpd;
namespace fs = std::filesystem;

/**
 * @brief getPowerSupplyFruPath
 */
void getPowerSupplyFruPath(vector<string>& powSuppFrus)
{
    auto bus = sdbusplus::bus::new_default();
    auto properties = bus.new_method_call(
        OBJECT_MAPPER_SERVICE, OBJECT_MAPPER_OBJECT,
        "xyz.openbmc_project.ObjectMapper", "GetSubTreePaths");
    properties.append(INVENTORY_PATH);
    properties.append(0);
    properties.append(std::array<const char*, 1>{POWER_SUPPLY_TYPE_INTERFACE});

    auto result = bus.call(properties);

    if (result.is_method_error())
    {
        throw runtime_error("Get api failed");
    }

    result.read(powSuppFrus);
}

/**
 * @brief Debugger
 * Displays the output in JSON.
 *
 * @param[in] output - json output to be displayed
 */
void debugger(json output)
{
    cout << output.dump(4) << '\n';
}

/**
 * @brief make Dbus Call
 *
 * @param[in] objectName - dbus Object
 * @param[in] interface - dbus Interface
 * @param[in] kw - keyword under the interface
 *
 * @return dbus call response
 */
auto makeDBusCall(const string& objectName, const string& interface,
                  const string& kw)
{
    auto bus = sdbusplus::bus::new_default();
    auto properties =
        bus.new_method_call(INVENTORY_MANAGER_SERVICE, objectName.c_str(),
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
 * Appends the type of the FRU and location code to the output
 *
 * @param[in] exIntf - extraInterfaces json from INVENTORY_JSON
 * @param[in] object - The D-Bus object to read the location code from
 * @param[out] kwVal - JSON object into which the FRU type and location code are
 *                     placed
 */
void addFruTypeAndLocation(json exIntf, const string& object, json& kwVal)
{
    if (object.find("powersupply") != string::npos)
    {
        kwVal.emplace("type", POWER_SUPPLY_TYPE_INTERFACE);
    }

    // add else if statement for fan fru

    else
    {
        for (auto intf : exIntf.items())
        {
            if ((intf.key().find("Item") != string::npos) &&
                (intf.value().is_null()))
            {
                kwVal.emplace("type", intf.key());
                break;
            }
        }
    }

    // Add location code.
    constexpr auto LOCATION_CODE_IF = "com.ibm.ipzvpd.Location";
    constexpr auto LOCATION_CODE_PROP = "LocationCode";

    try
    {
        variant<string> response;
        makeDBusCall(object, LOCATION_CODE_IF, LOCATION_CODE_PROP)
            .read(response);

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
 * @brief Get VINI properties
 * Making a dbus call for properties [SN, PN, CC, FN, DR]
 * under VINI interface.
 *
 * @param[in] invPath - Value of inventory Path
 * @param[in] exIntf - extraInterfaces json from INVENTORY_JSON
 *
 * @return json output which gives the properties under invPath's VINI interface
 */
json getVINIProperties(string invPath, json exIntf)
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
            makeDBusCall(objectName, interface, kw).read(response);

            if (auto vec = get_if<Binary>(&response))
            {
                kwVal.emplace(kw, string(vec->begin(), vec->end()));
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
 * @brief Get ExtraInterface Properties
 * Making a dbus call for those properties under extraInterfaces.
 *
 * @param[in] invPath - Value of inventory path
 * @param[in] extraInterface - One of the invPath's extraInterfaces whose value
 * 			       is not null
 * @param[in] prop - All properties of the extraInterface.
 *
 * @return json output which gives the properties under invPath's
 * 	   extraInterface.
 */
void getExtraInterfaceProperties(string invPath, string extraInterface,
                                 json prop, json exIntf, json& output)
{
    variant<string> response;
    string objectName = INVENTORY_PATH + invPath;

    for (auto itProp : prop.items())
    {
        string kw = itProp.key();
        try
        {
            makeDBusCall(objectName, extraInterface, kw).read(response);

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
 * Decides whether to make the dbus call for
 * getting properites from extraInterface or from
 * VINI interface, depending on the value of
 * extraInterfaces object in the inventory json.
 *
 * @param[in] itemEEPROM - holds the reference of one of the EEPROM objects.
 *
 * @return json output for one of the EEPROM objects.
 */
json interfaceDecider(json& itemEEPROM)
{
    if (itemEEPROM.find("inventoryPath") == itemEEPROM.end())
    {
        throw runtime_error("Inventory Path not found");
    }

    if (itemEEPROM.find("extraInterfaces") == itemEEPROM.end())
    {
        throw runtime_error("Extra Interfaces not found");
    }

    bool exIntfCheck = false;
    json output = json::object({});

    if (itemEEPROM.value("inherit", true))
    {
        json j = getVINIProperties(itemEEPROM.at("inventoryPath"),
                                   itemEEPROM["extraInterfaces"]);
        output.insert(j.begin(), j.end());
    }
    else
    {
        json js;
        for (auto ex : itemEEPROM["extraInterfaces"].items())
        {
            if (!(ex.value().is_null()))
            {
                exIntfCheck = true;
                getExtraInterfaceProperties(itemEEPROM.at("inventoryPath"),
                                            ex.key(), ex.value(),
                                            itemEEPROM["extraInterfaces"], js);
            }
        }
        output.emplace(itemEEPROM.at("inventoryPath"), js);
    }
    return output;
}

/**
 * @brief Parse Inventory JSON
 * Parses the complete inventory json and depending upon
 * the user option makes the dbuscall for the frus
 * via interfaceDecider function.
 *
 * @param[in] jsObject - Inventory json object
 * @param[in] flag - flag which tells about the user option(either dumpInventory
 * 		     or dumpObject)
 * @param[in] fruPath - fruPath is empty for dumpInventory option and holds
 * 			valid fruPath for dumpObject option.
 *
 * @return output json
 */
json parseInvJson(const json& jsObject, char flag, string fruPath)
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
                        if (itemEEPROM.find("inventoryPath") ==
                            itemEEPROM.end())
                        {
                            throw runtime_error("Inventory Path not found");
                        }

                        else if (itemEEPROM.at("inventoryPath") == fruPath)
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

void VpdTool::dumpInventory(const nlohmann::basic_json<>& jsObject)
{
    char flag = 'I';
    json output = json::array({});
    output.emplace_back(parseInvJson(jsObject, flag, ""));

    vector<string> powSuppFrus;

    getPowerSupplyFruPath(powSuppFrus);

    for (const auto& fru : powSuppFrus)
    {
        output.emplace_back(
            getVINIProperties(fru, nlohmann::detail::value_t::null));
    }

    debugger(output);
}

void VpdTool::dumpObject(const nlohmann::basic_json<>& jsObject)
{
    char flag = 'O';
    json output = json::array({});
    vector<string> powSuppFrus;

    getPowerSupplyFruPath(powSuppFrus);

    if (find(powSuppFrus.begin(), powSuppFrus.end(), fruPath) !=
        powSuppFrus.end())
    {
        output.emplace_back(
            getVINIProperties(fruPath, nlohmann::detail::value_t::null));
    }
    else
    {
        output.emplace_back(parseInvJson(jsObject, flag, fruPath));
    }
    debugger(output);
}

void VpdTool::readKeyword()
{
    string interface = "com.ibm.ipzvpd.";
    variant<Binary> response;

    try
    {
        json output = json::object({});
        json kwVal = json::object({});
        makeDBusCall(INVENTORY_PATH + fruPath, interface + recordName, keyword)
            .read(response);

        if (auto vec = get_if<Binary>(&response))
        {
            kwVal.emplace(keyword, string(vec->begin(), vec->end()));
        }

        output.emplace(fruPath, kwVal);

        debugger(output);
    }
    catch (json::exception& e)
    {
        json output = json::object({});
        json kwVal = json::object({});

        if (e.id == 316) // invalid UTF-8 byte exception
        {
            stringstream ss;
            string hexByte;
            string hexRep = "0x";
            ss << hexRep;
            hexByte = ss.str();

            // convert Decimal to Hex
            if (auto resp = get_if<Binary>(&response))
            {
                for (auto& vec : *resp)
                {
                    if ((int)vec == 0)
                    {
                        ss << std::hex << (int)vec;
                        hexByte = ss.str();
                    }
                    ss << std::hex << (int)vec;
                    hexByte = ss.str();
                }
            }

            kwVal.emplace(keyword, hexByte);
            output.emplace(fruPath, kwVal);

            debugger(output);
        }
    }
}

int VpdTool::updateKeyword()
{
    Binary val;

    if (value.find("0x") == string::npos)
    {
        val.assign(value.begin(), value.end());
    }
    else if (value.find("0x") != string::npos)
    {
        stringstream ss;
        ss.str(value.substr(2));
        string byteStr{};

        while (!ss.eof())
        {
            ss >> setw(2) >> byteStr;
            uint8_t byte = std::strtoul(byteStr.c_str(), nullptr, 16);

            val.push_back(byte);
        }
    }

    else
    {
        throw runtime_error("The value to be updated should be either in ascii "
                            "or in hex. Refer --help option");
    }

    // writeKeyword(fruPath, recordName, keyword, val);

    auto bus = sdbusplus::bus::new_default();
    auto properties =
        bus.new_method_call(BUSNAME, OBJPATH, IFACE, "WriteKeyword");
    properties.append(fruPath);
    properties.append(recordName);
    properties.append(keyword);
    properties.append(val);
    auto result = bus.call(properties);

    if (result.is_method_error())
    {
        throw runtime_error("Get api failed");
    }
    return 0;
}
