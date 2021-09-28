#include "vpd_tool_impl.hpp"

#include "vpd_exceptions.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sdbusplus/bus.hpp>
#include <variant>
#include <vector>

using namespace std;
using namespace openpower::vpd;
using namespace inventory;
using namespace openpower::vpd::manager::editor;
namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace openpower::vpd::exceptions;

Binary VpdTool::toBinary(const std::string& value)
{
    Binary val{};
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
            uint8_t byte = strtoul(byteStr.c_str(), nullptr, 16);

            val.push_back(byte);
        }
    }

    else
    {
        throw runtime_error("The value to be updated should be either in ascii "
                            "or in hex. Refer --help option");
    }
    return val;
}

void VpdTool::printReturnCode(int returnCode)
{
    if (returnCode)
    {
        cout << "\n Command failed with the return code " << returnCode
             << ". Continuing the execution. " << endl;
    }
}

string VpdTool::getPrintableValue(const vector<unsigned char>& vec)
{
    string str{};
    bool printableChar = true;
    for (auto i : vec)
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
        string hexRep = "0x";
        ss << hexRep;
        str = ss.str();

        // convert Decimal to Hex
        for (auto& v : vec)
        {
            ss << setfill('0') << setw(2) << hex << (int)v;
            str = ss.str();
        }
    }
    else
    {
        str = string(vec.begin(), vec.end());
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

json VpdTool::getVINIProperties(string invPath)
{
    variant<Binary> response;
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
        catch (const sdbusplus::exception::exception& e)
        {
            if (string(e.name()) ==
                string("org.freedesktop.DBus.Error.UnknownObject"))
            {
                kwVal.emplace(invPath, json::object({}));
                objFound = false;
                break;
            }
        }
    }

    return kwVal;
}

void VpdTool::getExtraInterfaceProperties(const string& invPath,
                                          const string& extraInterface,
                                          const json& prop, json& output)
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
        catch (const sdbusplus::exception::exception& e)
        {
            if (std::string(e.name()) ==
                std::string("org.freedesktop.DBus.Error.UnknownObject"))
            {
                objFound = false;
                break;
            }
            else if (std::string(e.name()) ==
                     std::string("org.freedesktop.DBus.Error.UnknownProperty"))
            {
                output.emplace(kw, "");
            }
        }
    }
}

static string getFruPresence(const string& objPath)
{
    string retVal{};
    auto bus = sdbusplus::bus::new_default();
    auto properties =
        bus.new_method_call(INVENTORY_MANAGER_SERVICE, objPath.c_str(),
                            "org.freedesktop.DBus.Properties", "Get");
    properties.append("xyz.openbmc_project.Inventory.Item");
    properties.append("Present");
    auto result = bus.call(properties);
    variant<bool> val;
    result.read(val);
    if (auto pVal = get_if<bool>(&val))
    {
        retVal = *pVal ? "true" : "false";
    }
    return retVal;
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

    json subOutput = json::object({});
    fruType = "FRU";

    json j;
    objFound = true;
    string invPath = itemEEPROM.at("inventoryPath");

    j = getVINIProperties(invPath);

    if (objFound)
    {
        subOutput.insert(j.begin(), j.end());
        json js;
        if (itemEEPROM.find("type") != itemEEPROM.end())
        {
            fruType = itemEEPROM.at("type");
        }
        js.emplace("TYPE", fruType);

        if (invPath.find("powersupply") != string::npos)
        {
            js.emplace("type", POWER_SUPPLY_TYPE_INTERFACE);
        }
        else if (invPath.find("fan") != string::npos)
        {
            js.emplace("type", FAN_INTERFACE);
        }

        for (const auto& ex : itemEEPROM["extraInterfaces"].items())
        {
            if (!(ex.value().is_null()))
            {
                getExtraInterfaceProperties(invPath, ex.key(), ex.value(), js);
            }
            if ((ex.key().find("Item") != string::npos) &&
                (ex.value().is_null()))
            {
                js.emplace("type", ex.key());
            }
            subOutput.insert(js.begin(), js.end());
        }
    }
    return subOutput;
}

json getPresentPropJson(const std::string& invPath, std::string& parentPresence)
{
    std::string presence = getFruPresence(invPath);
    if (parentPresence.empty())
    {
        parentPresence = presence;
    }
    json js;
    js.emplace("Present", presence);
    return js;
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
            string parentPresence{};
            for (auto itemEEPROM : itemFRUS.value())
            {
                json j = json::object({});
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
                            j = interfaceDecider(itemEEPROM);
                            json js = getPresentPropJson(
                                "/xyz/openbmc_project/inventory" + fruPath,
                                parentPresence);
                            j.insert(js.begin(), js.end());
                            output.emplace(fruPath, j);
                            return output;
                        }
                        else // this else is to keep track of parent present
                             // property.
                        {
                            json js = getPresentPropJson(
                                "/xyz/openbmc_project/inventory" +
                                    string(itemEEPROM.at("inventoryPath")),
                                parentPresence);
                        }
                    }
                    else
                    {
                        j = interfaceDecider(itemEEPROM);
                        json js = getPresentPropJson(
                            "/xyz/openbmc_project/inventory" +
                                string(itemEEPROM.at("inventoryPath")),
                            parentPresence);
                        j.insert(js.begin(), js.end());
                        output.emplace(string(itemEEPROM.at("inventoryPath")),
                                       j);
                    }
                }
                catch (const sdbusplus::exception::SdBusError& e)
                {
                    // if any of frupath doesn't have Present property of its
                    // own, emplace its parent's present property value.
                    if ((e.name() == std::string("org.freedesktop.DBus.Error."
                                                 "UnknownProperty") &&
                         (flag == 'O') && validObject) ||
                        (e.name() == std::string("org.freedesktop.DBus.Error."
                                                 "UnknownProperty") &&
                         (flag == 'I')))
                    {
                        json js;
                        js.emplace("Present", parentPresence);
                        j.insert(js.begin(), js.end());
                        output.emplace(string(itemEEPROM.at("inventoryPath")),
                                       j);
                    }

                    // for the user given child frupath which doesn't have
                    // Present prop (vpd-tool -o).
                    if ((flag == 'O') && validObject)
                    {
                        return output;
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

    cout.flush();
    string udevRemove = "udevadm trigger -c remove -s \"*nvmem*\" -v";
    int returnCode = system(udevRemove.c_str());
    printReturnCode(returnCode);

    string invManagerRestart =
        "systemctl restart xyz.openbmc_project.Inventory.Manager.service";
    returnCode = system(invManagerRestart.c_str());
    printReturnCode(returnCode);

    string sysVpdStop = "systemctl stop system-vpd.service";
    returnCode = system(sysVpdStop.c_str());
    printReturnCode(returnCode);

    string udevAdd = "udevadm trigger -c add -s \"*nvmem*\" -v";
    returnCode = system(udevAdd.c_str());
    printReturnCode(returnCode);
}

int VpdTool::updateHardware()
{
    int rc = 0;
    bool updCache = true;
    const Binary& val = static_cast<const Binary&>(toBinary(value));
    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    try
    {
        auto json = nlohmann::json::parse(inventoryJson);
        EditorImpl edit(fruPath, json, recordName, keyword);
        if (!((isPathInJson(fruPath)) &&
              (isRecKwInDbusJson(recordName, keyword))))
        {
            updCache = false;
        }
        edit.updateKeyword(val, updCache);
    }
    catch (json::parse_error& ex)
    {
        throw(VpdJsonException("Json Parsing failed", INVENTORY_JSON_SYM_LINK));
    }
    return rc;
}
