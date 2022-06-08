#include "vpd_tool_impl.hpp"

#include "impl.hpp"
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
using namespace openpower::vpd::parser;

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

        if (value.length() % 2 != 0)
        {
            throw runtime_error(
                "VPD-TOOL write option accepts 2 digit hex numbers. (Eg. 0x1 "
                "should be given as 0x01). Aborting the write operation.");
        }

        if (value.find_first_not_of("0123456789abcdefABCDEF", 2) !=
            std::string::npos)
        {
            throw runtime_error("Provide a valid hexadecimal input.");
        }

        while (ss >> setw(2) >> byteStr)
        {
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
                // TODO: Remove this if condition check once inventory json is
                // updated with xyz location code interface.
                if (ex.key() == "com.ibm.ipzvpd.Location")
                {
                    getExtraInterfaceProperties(
                        invPath,
                        "xyz.openbmc_project.Inventory.Decorator.LocationCode",
                        ex.value(), js);
                }
                else
                {
                    getExtraInterfaceProperties(invPath, ex.key(), ex.value(),
                                                js);
                }
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

json VpdTool::getPresentPropJson(const std::string& invPath,
                                 std::string& parentPresence)
{
    std::variant<bool> response;
    makeDBusCall(invPath, "xyz.openbmc_project.Inventory.Item", "Present")
        .read(response);

    std::string presence{};

    if (auto pVal = get_if<bool>(&response))
    {
        presence = *pVal ? "true" : "false";
        if (parentPresence.empty())
        {
            parentPresence = presence;
        }
    }
    else
    {
        presence = parentPresence;
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
                json subOutput = json::object({});
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
                            subOutput = interfaceDecider(itemEEPROM);
                            json presentJs = getPresentPropJson(
                                "/xyz/openbmc_project/inventory" + fruPath,
                                parentPresence);
                            subOutput.insert(presentJs.begin(),
                                             presentJs.end());
                            output.emplace(fruPath, subOutput);
                            return output;
                        }
                        else // this else is to keep track of parent present
                             // property.
                        {
                            json presentJs = getPresentPropJson(
                                "/xyz/openbmc_project/inventory" +
                                    string(itemEEPROM.at("inventoryPath")),
                                parentPresence);
                        }
                    }
                    else
                    {
                        subOutput = interfaceDecider(itemEEPROM);
                        json presentJs = getPresentPropJson(
                            "/xyz/openbmc_project/inventory" +
                                string(itemEEPROM.at("inventoryPath")),
                            parentPresence);
                        subOutput.insert(presentJs.begin(), presentJs.end());
                        output.emplace(string(itemEEPROM.at("inventoryPath")),
                                       subOutput);
                    }
                }
                catch (const sdbusplus::exception::SdBusError& e)
                {
                    // if any of frupath doesn't have Present property of its
                    // own, emplace its parent's present property value.
                    if (e.name() == std::string("org.freedesktop.DBus.Error."
                                                "UnknownProperty") &&
                        (((flag == 'O') && validObject) || flag == 'I'))
                    {
                        json presentJs;
                        presentJs.emplace("Present", parentPresence);
                        subOutput.insert(presentJs.begin(), presentJs.end());
                        output.emplace(string(itemEEPROM.at("inventoryPath")),
                                       subOutput);
                    }

                    // for the user given child frupath which doesn't have
                    // Present prop (vpd-tool -o).
                    if ((flag == 'O') && validObject)
                    {
                        return output;
                    }
                }
                catch (const exception& e)
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
    catch (const json::exception& e)
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

    string sysVpdRestart = "systemctl restart system-vpd.service";
    returnCode = system(sysVpdRestart.c_str());
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
        uint32_t offset = 0;
        EditorImpl edit(fruPath, json, recordName, keyword);
        if (!((isPathInJson(fruPath)) &&
              (isRecKwInDbusJson(recordName, keyword))))
        {
            updCache = false;
        }
        if (fruPath.starts_with("/sys/bus/spi"))
        {
            // TODO: Figure out a better way to get this, SPI eeproms
            // start at offset 0x30000
            offset = 0x30000;
        }
        edit.updateKeyword(val, offset, updCache);
    }
    catch (const json::parse_error& ex)
    {
        throw(VpdJsonException("Json Parsing failed", INVENTORY_JSON_SYM_LINK));
    }
    return rc;
}

static std::string getKwFromHw(const std::string& fruPath,
                               const std::string& recordName,
                               const std::string& keyword)
{
    uint32_t startOffset = 0;

    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    auto jsonFile = nlohmann::json::parse(inventoryJson);

    for (const auto& item : jsonFile["frus"][fruPath])
    {
        if (item.find("offset") != item.end())
        {
            startOffset = item["offset"];
        }
    }

    Binary completeVPDFile;
    completeVPDFile.resize(65504);
    fstream vpdFileStream;
    vpdFileStream.open(fruPath,
                       std::ios::in | std::ios::out | std::ios::binary);

    vpdFileStream.seekg(startOffset, ios_base::cur);
    vpdFileStream.read(reinterpret_cast<char*>(&completeVPDFile[0]), 65504);
    completeVPDFile.resize(vpdFileStream.gcount());
    vpdFileStream.clear(std::ios_base::eofbit);

    if (completeVPDFile.empty())
    {
        throw std::runtime_error("Invalid File");
    }

    Impl obj(completeVPDFile);
    return (obj.readKwFromHw(recordName, keyword));
}

void VpdTool::readKwFromHw()
{
    std::string keywordVal = getKwFromHw(fruPath, recordName, keyword);

    if (!keywordVal.empty())
    {
        json output = json::object({});
        json kwVal = json::object({});
        kwVal.emplace(keyword, keywordVal);

        output.emplace(fruPath, kwVal);

        debugger(output);
    }
    else
    {
        std::cerr << "The given keyword " << keyword << " or record "
                  << recordName
                  << " or both are not present in the given FRU path "
                  << fruPath << std::endl;
    }
}

int VpdTool::fixSystemVPD()
{
    std::cout << "\nRestorable record-keyword pairs and their data on cache & "
                 "hardware.\n";

    std::cout << "\n|=========================================================="
                 "============================================|"
              << std::endl;

    std::cout << left << setw(6) << "S.No" << left << setw(8) << "Record"
              << left << setw(9) << "Keyword" << left << setw(30)
              << "Data On Cache" << left << setw(30) << "Data On Hardware"
              << left << setw(14) << "Data Mismatch";

    std::cout << "\n|=========================================================="
                 "============================================|"
              << std::endl;

    int num = 0;
    std::vector<std::tuple<int, std::string, std::string, std::string,
                           std::string, std::string>>
        recKwData;

    for (const auto& recordKw : svpdKwdMap)
    {
        std::string record = recordKw.first;
        for (const auto& keyword : recordKw.second)
        {
            const auto busValue = readDBusProperty<std::variant<Binary>>(
                openpower::vpd::constants::pimIntf,
                "/xyz/openbmc_project/inventory/system/chassis/motherboard",
                openpower::vpd::constants::ipzVpdInf + record, keyword);

            const auto& hardwareValue = getKwFromHw(
                openpower::vpd::constants::systemVpdFilePath, record, keyword);

            std::ostringstream hwValStream;
            hwValStream << "0x";
            std::string hwValStr = hwValStream.str();

            for (uint16_t byte : hardwareValue)
            {
                hwValStream << std::setfill('0') << std::setw(2) << std::hex
                            << byte;
                hwValStr = hwValStream.str();
            }

            const auto busData = std::get_if<Binary>(&busValue);

            std::string mismatch = "NO"; // no mismatch

            if (busData != nullptr)
            {
                const auto busStr = byteArrayToHexString(*busData);
                if (busStr != hwValStr)
                {
                    mismatch = "YES";
                }
                recKwData.push_back(std::make_tuple(
                    ++num, record, keyword, busStr, hwValStr, mismatch));
                std::cout << left << setw(6) << num << left << setw(8) << record
                          << left << setw(9) << keyword << left << setw(30)
                          << setfill(' ') << busStr << left << setw(30)
                          << setfill(' ') << hwValStr << left << setw(14)
                          << mismatch;

                std::cout << "\n|----------------------------------------------"
                             "------------"
                             "--------------------------------------------|"
                          << std::endl;
            }
        }
    }

RETRY1:
    std::cout << "\n\nEnter 1 => If you choose cache data of all mismatching "
                 "record-keyword pairs\nEnter 2 => If you choose hardware data "
                 "of all mismatching record-keyword pairs\nEnter 3 => If you "
                 "wish to explore more options\nEnter 0 to exit  ";

    int option;
    std::cin >> option;

    std::cout << "\n|=========================================================="
                 "=============|"
              << std::endl;

    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK, std::ios::binary);

    if (!inventoryJson)
    {
        throw std::runtime_error("json file not found");
    }

    auto json = nlohmann::json::parse(inventoryJson);

    if (json.find("frus") == json.end())
    {
        std::cerr << "\nfrus not found in json";
    }
    if (option == 1)
    {
        for (const auto& data : recKwData)
        {
            if (std::get<5>(data) == "YES")
            {
                EditorImpl edit(openpower::vpd::constants::systemVpdFilePath,
                                json, std::get<1>(data), std::get<2>(data));
                edit.updateKeyword(toBinary(std::get<3>(data)), 0, true);
            }
        }
        std::cout
            << "\nData updated successfully for all mismatching record-keyword "
               "pairs by chosing their corresponding cache data.\n"
            << std::endl;

        return 0;
    }
    else if (option == 2)
    {
        for (const auto& data : recKwData)
        {
            if (std::get<5>(data) == "YES")
            {
                EditorImpl edit(openpower::vpd::constants::systemVpdFilePath,
                                json, std::get<1>(data), std::get<2>(data));
                edit.updateKeyword(toBinary(std::get<4>(data)), 0, true);
            }
        }
        std::cout
            << "\nData updated successfully for all mismatching record-keyword "
               "pairs by chosing their corresponding hardware data.\n"
            << std::endl;

        return 0;
    }
    else if (option == 3)
    {
        std::cout << "\nIterate through all restorable record-keyword pairs\n";

        for (const auto& data : recKwData)
        {
        RETRY2:
            std::cout << "\n|=================================================="
                         "========"
                         "============================================|"
                      << std::endl;

            std::cout << left << setw(6) << "S.No" << left << setw(8)
                      << "Record" << left << setw(9) << "Keyword" << left
                      << setw(30) << setfill(' ') << "Data On Cache" << left
                      << setw(30) << setfill(' ') << "Data On Hardware" << left
                      << setw(14) << "Data Mismatch" << std::endl;

            std::cout << left << setw(6) << std::get<0>(data) << left << setw(8)
                      << std::get<1>(data) << left << setw(9)
                      << std::get<2>(data) << left << setw(30) << setfill(' ')
                      << std::get<3>(data) << left << setw(30) << setfill(' ')
                      << std::get<4>(data) << left << setw(14)
                      << std::get<5>(data) << std::endl;

            std::cout << "\n|=================================================="
                         "========"
                         "============================================|"
                      << std::endl;

            if (std::get<5>(data) == "NO")
            {
                std::cout
                    << "No mismatch found. Skip this record-keyword pair.\n";
                continue;
            }

            std::cout << "\nEnter 4 => If you choose the cache value as the "
                         "right value\nEnter 5 => If you choose hardware value "
                         "as the right value\nEnter 6 => If you wish to enter "
                         "a new value to update both cache and hardware\nEnter "
                         "7 => If you wish to skip the above record-keyword "
                         "pair\nEnter 0 => To exit successfully  ";

            std::cin >> option;

            std::cout << "\n|=================================================="
                         "========"
                         "=============|"
                      << std::endl;

            EditorImpl edit(openpower::vpd::constants::systemVpdFilePath, json,
                            std::get<1>(data), std::get<2>(data));

            if (option == 4)
            {
                edit.updateKeyword(toBinary(std::get<3>(data)), 0, true);
            }
            else if (option == 5)
            {
                edit.updateKeyword(toBinary(std::get<4>(data)), 0, true);
            }
            else if (option == 6)
            {
                std::string value;
                std::cout << "\nEnter the new value to update both in cache & "
                             "hardware (Value should be in ASCII or in "
                             "HEX(prefixed with 0x)) : ";
                std::cin >> value;

                std::cout << "\n|=============================================="
                             "=========================|"
                          << std::endl;

                edit.updateKeyword(toBinary(value), 0, true);
            }
            else if (option == 7)
            {
                std::cout << "\nSkipped the above record-keyword pair. "
                             "Continueing to the next pair if available.\n";
                continue;
            }
            else if (option == 0)
            {
                std::cout << "\nExited successfully\n";
                exit(0);
            }
            else
            {
                std::cout << "\nProvide a valid option. Retrying for the "
                             "current record-keyword pair\n";
                goto RETRY2;
            }

            std::cout << "\nData updated successfully.\n";
        }
    }
    else if (option == 0)
    {
        std::cout << "\nExit successfully";
        exit(0);
    }
    else
    {
        std::cout << "\nProvide a valid option. Retry.";
        goto RETRY1;
    }
    return 0;
}
