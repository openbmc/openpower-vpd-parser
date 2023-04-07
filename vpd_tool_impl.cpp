#include "vpd_tool_impl.hpp"

#include "impl.hpp"
#include "parser_factory.hpp"
#include "vpd_exceptions.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sdbusplus/bus.hpp>
#include <variant>
#include <vector>

using namespace std;
using namespace inventory;
using namespace openpower::vpd::manager::editor;
namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace openpower::vpd::exceptions;
using namespace openpower::vpd::parser;

static void
    getVPDInMap(const std::string& vpdPath,
                std::unordered_map<std::string, DbusPropertyMap>& vpdMap,
                json& js, const std::string& invPath)
{
    auto jsonToParse = INVENTORY_JSON_DEFAULT;
    if (fs::exists(INVENTORY_JSON_SYM_LINK))
    {
        jsonToParse = INVENTORY_JSON_SYM_LINK;
    }

    std::ifstream inventoryJson(jsonToParse);
    if (!inventoryJson)
    {
        throw std::runtime_error("VPD JSON file not found");
    }

    try
    {
        js = json::parse(inventoryJson);
    }
    catch (const json::parse_error& ex)
    {
        throw std::runtime_error("VPD JSON parsing failed");
    }

    Binary vpdVector{};

    uint32_t vpdStartOffset = 0;
    vpdVector = getVpdDataInVector(js, constants::systemVpdFilePath);
    ParserInterface* parser = ParserFactory::getParser(
        vpdVector, invPath, constants::systemVpdFilePath, vpdStartOffset);
    auto parseResult = parser->parse();
    ParserFactory::freeParser(parser);

    if (auto pVal = std::get_if<Store>(&parseResult))
    {
        vpdMap = pVal->getVpdMap();
    }
    else
    {
        std::string err =
            vpdPath + " is not of type IPZ VPD. Unable to parse the VPD.";
        throw std::runtime_error(err);
    }
}

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
        catch (const sdbusplus::exception_t& e)
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
        catch (const sdbusplus::exception_t& e)
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

int VpdTool::updateHardware(const uint32_t offset)
{
    int rc = 0;
    const Binary& val = static_cast<const Binary&>(toBinary(value));
    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    try
    {
        auto json = nlohmann::json::parse(inventoryJson);
        EditorImpl edit(fruPath, json, recordName, keyword);

        edit.updateKeyword(val, offset, false);
    }
    catch (const json::parse_error& ex)
    {
        throw(VpdJsonException("Json Parsing failed", INVENTORY_JSON_SYM_LINK));
    }
    return rc;
}

void VpdTool::readKwFromHw(const uint32_t& startOffset)
{
    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    auto jsonFile = nlohmann::json::parse(inventoryJson);

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

    const std::string& inventoryPath =
        jsonFile["frus"][fruPath][0]["inventoryPath"];

    uint32_t vpdStartOffset = 0;
    for (const auto& item : jsonFile["frus"][fruPath])
    {
        if (item.find("offset") != item.end())
        {
            vpdStartOffset = item["offset"];
        }
    }

    Impl obj(completeVPDFile, (constants::pimPath + inventoryPath), fruPath,
             vpdStartOffset);
    std::string keywordVal = obj.readKwFromHw(recordName, keyword);

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

void VpdTool::printFixSystemVPDOption(UserOption option)
{
    switch (option)
    {
        case VpdTool::EXIT:
            cout << "\nEnter 0 => To exit successfully : ";
            break;
        case VpdTool::BACKUP_DATA_FOR_ALL:
            cout << "\n\nEnter 1 => If you choose the data on backup for all "
                    "mismatching record-keyword pairs";
            break;
        case VpdTool::SYSTEM_BACKPLANE_DATA_FOR_ALL:
            cout << "\nEnter 2 => If you choose the data on primary for all "
                    "mismatching record-keyword pairs";
            break;
        case VpdTool::MORE_OPTIONS:
            cout << "\nEnter 3 => If you wish to explore more options";
            break;
        case VpdTool::BACKUP_DATA_FOR_CURRENT:
            cout << "\nEnter 4 => If you choose the data on backup as the "
                    "right value";
            break;
        case VpdTool::SYSTEM_BACKPLANE_DATA_FOR_CURRENT:
            cout << "\nEnter 5 => If you choose the data on primary as the "
                    "right value";
            break;
        case VpdTool::NEW_VALUE_ON_BOTH:
            cout << "\nEnter 6 => If you wish to enter a new value to update "
                    "both on backup and primary";
            break;
        case VpdTool::SKIP_CURRENT:
            cout << "\nEnter 7 => If you wish to skip the above "
                    "record-keyword pair";
            break;
    }
}

void VpdTool::getSystemDataFromCache(IntfPropMap& svpdBusData)
{
    const auto vsys = getAllDBusProperty<GetAllResultType>(
        constants::pimIntf,
        "/xyz/openbmc_project/inventory/system/chassis/motherboard",
        "com.ibm.ipzvpd.VSYS");
    svpdBusData.emplace("VSYS", vsys);

    const auto vcen = getAllDBusProperty<GetAllResultType>(
        constants::pimIntf,
        "/xyz/openbmc_project/inventory/system/chassis/motherboard",
        "com.ibm.ipzvpd.VCEN");
    svpdBusData.emplace("VCEN", vcen);

    const auto lxr0 = getAllDBusProperty<GetAllResultType>(
        constants::pimIntf,
        "/xyz/openbmc_project/inventory/system/chassis/motherboard",
        "com.ibm.ipzvpd.LXR0");
    svpdBusData.emplace("LXR0", lxr0);

    const auto util = getAllDBusProperty<GetAllResultType>(
        constants::pimIntf,
        "/xyz/openbmc_project/inventory/system/chassis/motherboard",
        "com.ibm.ipzvpd.UTIL");
    svpdBusData.emplace("UTIL", util);
}

/*
 * @brief This API retrieves the hardware backup in map
 *
 * @param[in] systemVpdBckupPath - The path to the EEPROM file that stores the
 * primary VPD data.
 * @param[in] backupVpdInvPath - Inventory object path.
 * @param[in] jsonFile - Reference to vpd inventory json object.
 * @param[out] backupVpdMap - An IPZ VPD map containing the parsed Backup VPD.
 * */
void getBackupVpdInMap(const std::string& systemVpdBckupPath,
                       const std::string& backupVpdInvPath,
                       nlohmann::json& jsonFile, Parsed& backupVpdMap)
{
    if (!fs::exists(systemVpdBckupPath))
    {
        std::cout << "Backup is on hardware EEPORM file,"
                     " but the path of EEPROM file "
                  << systemVpdBckupPath << " does not exist." << std::endl;

        exit(0);
    }
    else
    {
        ParserInterface* parser = nullptr;
        try
        {
            auto backupVpdParseResult = parseVpdFile(
                systemVpdBckupPath, backupVpdInvPath, jsonFile, parser);

            // release the parser object
            ParserFactory::freeParser(parser);

            if (auto pVal = get_if<Store>(&backupVpdParseResult))
            {
                backupVpdMap = pVal->getVpdMap();
            }
            else
            {
                std::cerr << systemVpdBckupPath
                          << " is not of type IPZ VPD."
                             " Unable to parse the VPD."
                          << std::endl;

                exit(0);
            }
        }
        catch (const VpdEccException& ex)
        {
            std::cerr << "ECC Exceptions found in the Backup VPD file "
                      << systemVpdBckupPath << std::endl;

            cerr << ex.what() << std::endl;

            // release the parser object
            ParserFactory::freeParser(parser);

            exit(0);
        }
    }
}

int VpdTool::fixSystemVPD()
{
    std::string outline(191, '=');
    cout << "\nRestorable record-keyword pairs and their data on backup & "
            "primary.\n\n"
         << outline << std::endl;

    cout << left << setw(6) << "S.No" << left << setw(8) << "Record" << left
         << setw(9) << "Keyword" << left << setw(75) << "Data On Backup" << left
         << setw(75) << "Data On Primary" << left << setw(14)
         << "Data Mismatch\n"
         << outline << std::endl;

    int num = 0;

    // Get system VPD data in map
    unordered_map<string, DbusPropertyMap> vpdMap;
    json js;
    getVPDInMap(constants::systemVpdFilePath, vpdMap, js,
                constants::pimPath +
                    static_cast<std::string>(constants::SYSTEM_OBJECT));

    // Get the value of systemVpdBckupPath field from json
    const std::string& systemVpdBckupPath =
        js["frus"][constants::systemVpdFilePath].at(0).value(
            "systemVpdBckupPath", "");

    Parsed backupVpdMap{};
    bool isbackupOnCache = true;

    // Get system VPD D-Bus Data in a map
    IntfPropMap svpdBusData;
    if (systemVpdBckupPath.empty())
    {
        getSystemDataFromCache(svpdBusData);
    }
    else
    {
        isbackupOnCache = false;
        const std::string& backupVpdInvPath =
            js["frus"][systemVpdBckupPath][0]["inventoryPath"]
                .get_ref<const nlohmann::json::string_t&>();

        getBackupVpdInMap(systemVpdBckupPath, backupVpdInvPath, js,
                          backupVpdMap);
    }

    for (const auto& recordKw : svpdKwdMap)
    {
        string record = recordKw.first;

        if (systemVpdBckupPath.empty())
        {
            // Extract specific record data from the svpdBusData map.
            const auto& rec = svpdBusData.find(record);
            if (rec == svpdBusData.end())
            {
                std::cerr << record
                          << " not a part of critical system VPD records."
                          << std::endl;
                continue;
            }
        }

        std::string primaryVpdKwValueStr{}, backupVpdKwValueStr{};
        string backupVpdRecName{}, backupVpdKwName{};

        for (const auto& keywordInfo : recordKw.second)
        {
            const auto& keyword = get<0>(keywordInfo);
            string mismatch = "NO"; // no mismatch
            string primaryVpdKwValue{};
            string backupVpdKwValue{};
            auto recItr = vpdMap.find(record);

            if (recItr != vpdMap.end())
            {
                DbusPropertyMap& kwValMap = recItr->second;
                auto kwItr = kwValMap.find(keyword);
                if (kwItr != kwValMap.end())
                {
                    primaryVpdKwValue = kwItr->second;
                }
            }

            inventory::Value kwValue;
            if (systemVpdBckupPath.empty())
            {
                const auto& recData = svpdBusData.find(record)->second;
                for (auto& kwData : recData)
                {
                    if (kwData.first == keyword)
                    {
                        kwValue = kwData.second;
                        break;
                    }
                }
            }
            else
            {
                backupVpdRecName = get<4>(keywordInfo);
                backupVpdKwName = get<5>(keywordInfo);

                auto recItr = backupVpdMap.find(backupVpdRecName);

                if (recItr != backupVpdMap.end())
                {
                    backupVpdRecKwdValMap& backupVpdKwValueMap = recItr->second;

                    auto kwItr = backupVpdKwValueMap.find(backupVpdKwName);

                    if (kwItr != backupVpdKwValueMap.end())
                    {
                        backupVpdKwValue = kwItr->second;
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    continue;
                }
            }

            if (keyword != "SE")
            {
                ostringstream primaryVpdKwValueStream;
                primaryVpdKwValueStream << "0x";
                primaryVpdKwValueStr = primaryVpdKwValueStream.str();

                for (uint16_t byte : primaryVpdKwValue)
                {
                    primaryVpdKwValueStream << setfill('0') << setw(2) << hex
                                            << byte;
                    primaryVpdKwValueStr = primaryVpdKwValueStream.str();
                }

                if (systemVpdBckupPath.empty())
                {
                    if (const auto value = get_if<Binary>(&kwValue))
                    {
                        backupVpdKwValueStr = byteArrayToHexString(*value);
                    }
                }
                else
                {
                    ostringstream backupKwValStream;
                    backupKwValStream << "0x";
                    backupVpdKwValueStr = backupKwValStream.str();

                    for (uint16_t byte : backupVpdKwValue)
                    {
                        backupKwValStream << setfill('0') << setw(2) << hex
                                          << byte;

                        backupVpdKwValueStr = backupKwValStream.str();
                    }
                }

                if (backupVpdKwValueStr != primaryVpdKwValueStr)
                {
                    mismatch = "YES";
                }
            }
            else
            {
                if (systemVpdBckupPath.empty())
                {
                    if (const auto value = get_if<Binary>(&kwValue))
                    {
                        backupVpdKwValueStr = getPrintableValue(*value);
                    }
                }
                else
                {
                    backupVpdKwValueStr = backupVpdKwValue;
                }

                if (backupVpdKwValueStr != primaryVpdKwValue)
                {
                    mismatch = "YES";
                }

                primaryVpdKwValueStr = primaryVpdKwValue;
            }

            recKwData.push_back(make_tuple(++num, record, keyword,
                                           backupVpdKwValueStr,
                                           primaryVpdKwValueStr, mismatch,
                                           backupVpdRecName, backupVpdKwName));

            std::string splitLine(191, '-');
            cout << left << setw(6) << num << left << setw(8) << record << left
                 << setw(9) << keyword << left << setw(75) << setfill(' ')
                 << backupVpdKwValueStr << left << setw(75) << setfill(' ')
                 << primaryVpdKwValueStr << left << setw(14) << mismatch << '\n' 
                 << splitLine << endl;
        }
    }
    parseSVPDOptions(js, isbackupOnCache, systemVpdBckupPath);
    return 0;
}

void VpdTool::parseSVPDOptions(const nlohmann::json& json,
                               const bool& isbackupOnCache,
                               const std::string& backupVpdFilePath)
{
    do
    {
        printFixSystemVPDOption(VpdTool::BACKUP_DATA_FOR_ALL);
        printFixSystemVPDOption(VpdTool::SYSTEM_BACKPLANE_DATA_FOR_ALL);
        printFixSystemVPDOption(VpdTool::MORE_OPTIONS);
        printFixSystemVPDOption(VpdTool::EXIT);

        int option = 0;
        cin >> option;

        std::string outline(191, '=');
        cout << '\n' << outline << endl;

        if (json.find("frus") == json.end())
        {
            throw runtime_error("Frus not found in json");
        }

        bool mismatchFound = false;

        if (option == VpdTool::BACKUP_DATA_FOR_ALL)
        {
            for (const auto& data : recKwData)
            {
                if (get<5>(data) == "YES")
                {
                    EditorImpl edit(constants::systemVpdFilePath, json,
                                    get<1>(data), get<2>(data));
                    edit.updateKeyword(toBinary(get<3>(data)), 0, true);
                    mismatchFound = true;
                }
            }

            if (mismatchFound)
            {
                cout << "\nData updated successfully for all mismatching "
                        "record-keyword pairs by choosing their corresponding "
                        "data from backup. Exit successfully.\n"
                     << endl;
            }
            else
            {
                cout << "\nNo mismatch found for any of the above mentioned "
                        "record-keyword pair. Exit successfully.\n";
            }

            exit(0);
        }
        else if (option == VpdTool::SYSTEM_BACKPLANE_DATA_FOR_ALL)
        {
            for (const auto& data : recKwData)
            {
                if (get<5>(data) == "YES")
                {
                    std::string vpdFilePath{};
                    std::string recordName{};
                    std::string kwName{};

                    if (isbackupOnCache)
                    {
                        vpdFilePath = constants::systemVpdFilePath;
                        recordName = get<1>(data);
                        kwName = get<2>(data);
                    }
                    else
                    {
                        vpdFilePath = backupVpdFilePath;
                        recordName = get<6>(data);
                        kwName = get<7>(data);
                    }

                    EditorImpl edit(vpdFilePath, json, recordName, kwName);

                    edit.updateKeyword(toBinary(get<4>(data)), 0, true);
                    mismatchFound = true;
                }
            }

            if (mismatchFound)
            {
                cout << "\nData updated successfully for all mismatching "
                        "record-keyword pairs by choosing their corresponding "
                        "data from primary VPD.\n"
                     << endl;
            }
            else
            {
                cout << "\nNo mismatch found for any of the above mentioned "
                        "record-keyword pair. Exit successfully.\n";
            }

            exit(0);
        }
        else if (option == VpdTool::MORE_OPTIONS)
        {
            cout << "\nIterate through all restorable record-keyword pairs\n";

            for (const auto& data : recKwData)
            {
                do
                {
                    cout << '\n' << outline << endl;

                    cout << left << setw(6) << "S.No" << left << setw(8)
                         << "Record" << left << setw(9) << "Keyword" << left
                         << setw(75) << setfill(' ') << "Backup Data" << left
                         << setw(75) << setfill(' ') << "Primary Data" << left
                         << setw(14) << "Data Mismatch" << endl;

                    cout << left << setw(6) << get<0>(data) << left << setw(8)
                         << get<1>(data) << left << setw(9) << get<2>(data)
                         << left << setw(75) << setfill(' ') << get<3>(data)
                         << left << setw(75) << setfill(' ') << get<4>(data)
                         << left << setw(14) << get<5>(data);

                    cout << '\n' << outline << endl;

                    if (get<5>(data) == "NO")
                    {
                        cout << "\nNo mismatch found.\n";
                        printFixSystemVPDOption(VpdTool::NEW_VALUE_ON_BOTH);
                        printFixSystemVPDOption(VpdTool::SKIP_CURRENT);
                        printFixSystemVPDOption(VpdTool::EXIT);
                    }
                    else
                    {
                        printFixSystemVPDOption(
                            VpdTool::BACKUP_DATA_FOR_CURRENT);
                        printFixSystemVPDOption(
                            VpdTool::SYSTEM_BACKPLANE_DATA_FOR_CURRENT);
                        printFixSystemVPDOption(VpdTool::NEW_VALUE_ON_BOTH);
                        printFixSystemVPDOption(VpdTool::SKIP_CURRENT);
                        printFixSystemVPDOption(VpdTool::EXIT);
                    }

                    cin >> option;
                    cout << '\n' << outline << endl;

                    EditorImpl edit(constants::systemVpdFilePath, json,
                                    get<1>(data), get<2>(data));

                    EditorImpl backupVpdEditObj(backupVpdFilePath, json,
                                                get<6>(data), get<7>(data));

                    if (option == VpdTool::BACKUP_DATA_FOR_CURRENT)
                    {
                        edit.updateKeyword(toBinary(get<3>(data)), 0, true);
                        cout << "\nData updated successfully.\n";
                        break;
                    }
                    else if (option ==
                             VpdTool::SYSTEM_BACKPLANE_DATA_FOR_CURRENT)
                    {
                        if (isbackupOnCache)
                        {
                            edit.updateKeyword(toBinary(get<4>(data)), 0, true);
                        }
                        else
                        {
                            backupVpdEditObj.updateKeyword(
                                toBinary(get<4>(data)), 0, true);
                        }
                        cout << "\nData updated successfully.\n";
                        break;
                    }
                    else if (option == VpdTool::NEW_VALUE_ON_BOTH)
                    {
                        string value;
                        cout << "\nEnter the new value to update on both "
                                "primary & backup. Value should be in ASCII or "
                                "in HEX(prefixed with 0x) : ";
                        cin >> value;
                        cout << '\n' << outline << endl;

                        edit.updateKeyword(toBinary(value), 0, true);

                        if (!isbackupOnCache)
                        {
                            backupVpdEditObj.updateKeyword(toBinary(value), 0,
                                                           true);
                        }
                        cout << "\nData updated successfully.\n";
                        break;
                    }
                    else if (option == VpdTool::SKIP_CURRENT)
                    {
                        cout << "\nSkipped the above record-keyword pair. "
                                "Continue to the next available pair.\n";
                        break;
                    }
                    else if (option == VpdTool::EXIT)
                    {
                        cout << "\nExit successfully\n";
                        exit(0);
                    }
                    else
                    {
                        cout << "\nProvide a valid option. Retrying for the "
                                "current record-keyword pair\n";
                    }
                } while (1);
            }
            exit(0);
        }
        else if (option == VpdTool::EXIT)
        {
            cout << "\nExit successfully";
            exit(0);
        }
        else
        {
            cout << "\nProvide a valid option. Retry.";
            continue;
        }

    } while (true);
}

int VpdTool::cleanSystemVPD()
{
    try
    {
        // Get system VPD hardware data in map
        unordered_map<string, DbusPropertyMap> vpdMap;
        json js;
        getVPDInMap(constants::systemVpdFilePath, vpdMap, js,
                    constants::pimPath +
                        static_cast<std::string>(constants::SYSTEM_OBJECT));

        // Get the value of systemVpdBckupPath field from json
        const std::string& systemVpdBckupPath =
            js["frus"][constants::systemVpdFilePath].at(0).value(
                "systemVpdBckupPath", "");

        RecKwValMap kwdsToBeUpdated;

        for (auto recordMap : svpdKwdMap)
        {
            const auto& record = recordMap.first;
            std::unordered_map<std::string, Binary> kwDefault;
            for (auto keywordMap : recordMap.second)
            {
                // Skip those keywords which cannot be reset at manufacturing
                if (!std::get<3>(keywordMap))
                {
                    continue;
                }
                const auto& keyword = std::get<0>(keywordMap);

                // Get hardware value for this keyword from vpdMap
                Binary hardwareValue;

                auto recItr = vpdMap.find(record);

                if (recItr != vpdMap.end())
                {
                    DbusPropertyMap& kwValMap = recItr->second;
                    auto kwItr = kwValMap.find(keyword);
                    if (kwItr != kwValMap.end())
                    {
                        hardwareValue = toBinary(kwItr->second);
                    }
                }

                // compare hardware value with the keyword's default value
                auto defaultValue = std::get<1>(keywordMap);
                if (hardwareValue != defaultValue)
                {
                    EditorImpl edit(constants::systemVpdFilePath, js, record,
                                    keyword);
                    edit.updateKeyword(defaultValue, 0, true);

                    if (!systemVpdBckupPath.empty())
                    {
                        if (!fs::exists(systemVpdBckupPath))
                        {
                            std::cout
                                << "Backup is on hardware EEPORM file,"
                                   " but the path of EEPROM file "
                                << systemVpdBckupPath
                                << " does not exist. So not able to reset "
                                   "the keyword value on Backup"
                                << std::endl;
                        }
                        else
                        {

                            EditorImpl backupVpdEditObj(
                                systemVpdBckupPath, js, std::get<4>(keywordMap),
                                std::get<5>(keywordMap));

                            backupVpdEditObj.updateKeyword(defaultValue, 0,
                                                           true);
                        }
                    }
                }
            }
        }

        std::cout
            << "\n The critical keywords from system backplane VPD has been "
               "reset successfully."
            << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what();
        std::cerr
            << "\nManufacturing reset on system vpd keywords is unsuccessful";
    }
    return 0;
}
