#include "vpd_tool_impl.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sdbusplus/bus.hpp>
#include <variant>
#include <vector>

using namespace std;
using sdbusplus::exception::SdBusError;
using namespace openpower::vpd;
using namespace inventory;
using namespace openpower::vpd::manager::editor;
namespace fs = std::filesystem;

string VpdTool::getPrintableValue(const vector<unsigned char>& vector)
{
    string str{};
    bool printableChar = true;
    for (auto i : vector)
    {
        if (!isprint(i))
        {
            printableChar = false;
            break;
        }
    }

    if (!printableChar)
    {
        stringstream ss;
        string hexByte{};
        string hexRep = "0x";
        ss << hexRep;
        hexByte = ss.str();

        // convert Decimal to Hex
        for (auto& vec : vector)
        {
            ss << setfill('0') << setw(2) << hex << (int)vec;
            hexByte = ss.str();
        }
        return hexByte;
    }
    else
    {
        str = string(vector.begin(), vector.end());
    }
    return str;
}

void VpdTool::eraseInventoryPath(string& fru)
{
    // Power supply frupath comes with INVENTORY_PATH appended in prefix.
    // Stripping it off inorder to avoid INVENTORY_PATH duplication
    // during getVINIProperties() execution.
    fru.erase(0, sizeof(INVENTORY_PATH) - 1);
}

void VpdTool::debugger(json output)
{
    cout << output.dump(4) << '\n';
}

auto VpdTool::makeDBusCall(const string& objectName, const string& interface,
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

void VpdTool::addFruTypeAndLocation(json exIntf, const string& object,
                                    json& kwVal)
{
    if (object.find("powersupply") != string::npos)
    {
        kwVal.emplace("type", POWER_SUPPLY_TYPE_INTERFACE);
    }

    // add else if statement for fan fru

    else
    {
        for (const auto& intf : exIntf.items())
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

json VpdTool::getVINIProperties(string invPath, json exIntf)
{
    variant<Binary> response;
    json output = json::object({});
    json kwVal = json::object({});

    vector<string> keyword{"CC", "SN", "PN", "FN", "DR"};
    string interface = "com.ibm.ipzvpd.VINI";
    string objectName = {};

    if (invPath.find(INVENTORY_PATH) != string::npos)
    {
        objectName = invPath;
        eraseInventoryPath(invPath);
    }
    else
    {
        objectName = INVENTORY_PATH + invPath;
    }

    for (string kw : keyword)
    {
        try
        {
            makeDBusCall(objectName, interface, kw).read(response);

            if (auto vec = get_if<Binary>(&response))
            {
                string printableVal = getPrintableValue(*vec);
                kwVal.emplace(kw, printableVal);
            }
        }
        catch (const SdBusError& e)
        {
            output.emplace(invPath, json::object({}));
        }
    }

    addFruTypeAndLocation(exIntf, objectName, kwVal);
    kwVal.emplace("TYPE", fruType);

    output.emplace(invPath, kwVal);
    return output;
}

void VpdTool::getExtraInterfaceProperties(string invPath, string extraInterface,
                                          json prop, json exIntf, json& output)
{
    variant<string> response;

    string objectName = INVENTORY_PATH + invPath;

    for (const auto& itProp : prop.items())
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

json VpdTool::interfaceDecider(json& itemEEPROM)
{
    if (itemEEPROM.find("inventoryPath") == itemEEPROM.end())
    {
        throw runtime_error("Inventory Path not found");
    }

    if (itemEEPROM.find("extraInterfaces") == itemEEPROM.end())
    {
        throw runtime_error("Extra Interfaces not found");
    }

    json output = json::object({});
    fruType = "FRU";

    // check type and add FRU Type in object
    if (itemEEPROM.find("type") != itemEEPROM.end())
    {
        fruType = itemEEPROM.at("type");
    }

    if (itemEEPROM.value("inherit", true))
    {
        json j = getVINIProperties(itemEEPROM.at("inventoryPath"),
                                   itemEEPROM["extraInterfaces"]);
        output.insert(j.begin(), j.end());
    }
    else
    {
        json js;
        for (const auto& ex : itemEEPROM["extraInterfaces"].items())
        {
            if (!(ex.value().is_null()))
            {
                getExtraInterfaceProperties(itemEEPROM.at("inventoryPath"),
                                            ex.key(), ex.value(),
                                            itemEEPROM["extraInterfaces"], js);
            }
        }
        output.emplace(itemEEPROM.at("inventoryPath"), js);
    }

    return output;
}

json VpdTool::parseInvJson(const json& jsObject, char flag, string fruPath)
{
    json output = json::object({});
    bool validObject = false;

    if (jsObject.find("frus") == jsObject.end())
    {
        throw runtime_error("Frus missing in Inventory json");
    }
    else
    {
        for (const auto& itemFRUS : jsObject["frus"].items())
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
    debugger(output);
}

void VpdTool::dumpObject(const nlohmann::basic_json<>& jsObject)
{
    char flag = 'O';
    json output = json::array({});
    output.emplace_back(parseInvJson(jsObject, flag, fruPath));
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

        string printableVal{};
        if (auto vec = get_if<Binary>(&response))
        {
            printableVal = getPrintableValue(*vec);
        }
        kwVal.emplace(keyword, printableVal);

        output.emplace(fruPath, kwVal);

        debugger(output);
    }
    catch (json::exception& e)
    {
        json output = json::object({});
        json kwVal = json::object({});
    }
}

int VpdTool::updateKeyword()
{
    Binary val = toBinary(value);
    auto bus = sdbusplus::bus::new_default();
    auto properties =
        bus.new_method_call(BUSNAME, OBJPATH, IFACE, "WriteKeyword");
    properties.append(static_cast<sdbusplus::message::object_path>(fruPath));
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

void VpdTool::forceReset(const nlohmann::basic_json<>& jsObject)
{
    for (const auto& itemFRUS : jsObject["frus"].items())
    {
        for (const auto& itemEEPROM : itemFRUS.value().items())
        {
            string fru = itemEEPROM.value().at("inventoryPath");

            fs::path fruCachePath = INVENTORY_MANAGER_CACHE;
            fruCachePath += INVENTORY_PATH;
            fruCachePath += fru;

            try
            {
                for (const auto& it : fs::directory_iterator(fruCachePath))
                {
                    if (fs::is_regular_file(it.status()))
                    {
                        fs::remove(it);
                    }
                }
            }
            catch (const fs::filesystem_error& e)
            {
            }
        }
    }

    string udevRemove = "udevadm trigger -c remove -s \"*nvmem*\" -v";
    system(udevRemove.c_str());

    string invManagerRestart =
        "systemctl restart xyz.openbmc_project.Inventory.Manager.service";
    system(invManagerRestart.c_str());

    string sysVpdStop = "systemctl stop system-vpd.service";
    system(sysVpdStop.c_str());

    string udevAdd = "udevadm trigger -c add -s \"*nvmem*\" -v";
    system(udevAdd.c_str());
}

int VpdTool::updateHardware()
{
    int rc = 0;
    Binary val = toBinary(value);
    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    auto json = nlohmann::json::parse(inventoryJson);
    EditorImpl edit(fruPath, json, recordName, keyword);
    if (!((eepromPresenceInJson(fruPath)) &&
          (recKwPresenceInDbusProp(recordName, keyword))))
    {
        edit.updCache = false;
    }
    edit.updateKeyword(val);
    return rc;
}
