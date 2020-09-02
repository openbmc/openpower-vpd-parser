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
using sdbusplus::exception::SdBusError;
using namespace openpower::vpd;
namespace fs = std::filesystem;

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
        for (const auto& ex : itemEEPROM["extraInterfaces"].items())
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

bool VpdTool::otherCntrlCodes(int c)
{
    if (c >= 16 && c <= 31)
    {
        return true;
    }
    return false;
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
            bool printableChar = true;
            std::vector<unsigned char>& vector = *vec;
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
                string hexByte;
                string hexRep = "0x";
                ss << hexRep;
                hexByte = ss.str();

                // convert Decimal to Hex
                if (auto resp = get_if<Binary>(&response))
                {
                    for (auto& vec : *resp)
                    {
                        if ((iscntrl(vec)) && (!otherCntrlCodes((int)vec)))
                        {
                            ss << hex << 0;
                            hexByte = ss.str();
                        }
                        ss << hex << (int)vec;
                        hexByte = ss.str();
                    }
                }
                kwVal.emplace(keyword, hexByte);
            }
            else
            {
                string str = string(vec->begin(), vec->end());
                kwVal.emplace(keyword, str);
            }
        }
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
            uint8_t byte = strtoul(byteStr.c_str(), nullptr, 16);

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
