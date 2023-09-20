#include "vpd_tool_impl.hpp"

#include "impl.hpp"
#include "parser_factory.hpp"
#include "vpd_exceptions.hpp"

#include <sdbusplus/bus.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
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
using namespace openpower::vpd::parser::factory;
using namespace openpower::vpd::parser::interface;

bool VpdTool::fileToVector(Binary& data)
{
    try
    {
        std::ifstream file(value, std::ifstream::in);

        if (file)
        {
            std::string line;
            while (std::getline(file, line))
            {
                std::istringstream iss(line);
                std::string byteStr;
                while (iss >> std::setw(2) >> std::hex >> byteStr)
                {
                    uint8_t byte = strtoul(byteStr.c_str(), nullptr, 16);
                    data.emplace(data.end(), byte);
                }
            }
            return true;
        }
        else
        {
            std::cerr << "Unable to open the given file " << value << std::endl;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what();
    }
    return false;
}

bool VpdTool::copyStringToFile(const std::string& input)
{
    try
    {
        std::ofstream outFile(value, std::ofstream::out);

        if (outFile.is_open())
        {
            std::string hexString = input;
            if (input.substr(0, 2) == "0x")
            {
                // truncating prefix 0x
                hexString = input.substr(2);
            }
            outFile.write(hexString.c_str(), hexString.length());
        }
        else
        {
            std::cerr << "Error opening output file " << value << std::endl;
            return false;
        }

        outFile.close();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what();
        return false;
    }
    return true;
}

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
    vpdVector = getVpdDataInVector(js, vpdPath);
    ParserInterface* parser = ParserFactory::getParser(vpdVector, invPath,
                                                       vpdPath, vpdStartOffset);
    auto parseResult = parser->parse();
    ParserFactory::freeParser(parser);

    if (auto pVal = std::get_if<Store>(&parseResult))
    {
        vpdMap = pVal->getVpdMap();
    }
    else
    {
        std::string err = vpdPath +
                          " is not of type IPZ VPD. Unable to parse the VPD.";
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
            // Properties under Decorator.Asset interface are derived from VINI
            // keywords. Displaying VINI keywords and skipping Decorator.Asset
            // interface's properties will avoid duplicate entries in vpd-tool
            // output.
            if (ex.key() == "xyz.openbmc_project.Inventory.Decorator.Asset" &&
                itemEEPROM["extraInterfaces"].find(constants::kwdVpdInf) !=
                    itemEEPROM["extraInterfaces"].end())
            {
                continue;
            }

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

json VpdTool::getPresentPropJson(const std::string& invPath)
{
    std::variant<bool> response;
    std::string presence = "Unknown";

    try
    {
        makeDBusCall(invPath, "xyz.openbmc_project.Inventory.Item", "Present")
            .read(response);

        if (auto pVal = get_if<bool>(&response))
        {
            presence = *pVal ? "true" : "false";
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        presence = "Unknown";
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
                                "/xyz/openbmc_project/inventory" + fruPath);
                            subOutput.insert(presentJs.begin(),
                                             presentJs.end());
                            output.emplace(fruPath, subOutput);
                            return output;
                        }
                    }
                    else
                    {
                        subOutput = interfaceDecider(itemEEPROM);
                        json presentJs = getPresentPropJson(
                            "/xyz/openbmc_project/inventory" +
                            string(itemEEPROM.at("inventoryPath")));
                        subOutput.insert(presentJs.begin(), presentJs.end());
                        output.emplace(string(itemEEPROM.at("inventoryPath")),
                                       subOutput);
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
    const std::string& kw = getDbusNameForThisKw(keyword);

    string interface = "com.ibm.ipzvpd.";
    variant<Binary> response;

    try
    {
        makeDBusCall(INVENTORY_PATH + fruPath, interface + recordName, kw)
            .read(response);

        string printableVal{};
        if (auto vec = get_if<Binary>(&response))
        {
            printableVal = getPrintableValue(*vec);
        }

        if (!value.empty())
        {
            if (copyStringToFile(printableVal))
            {
                std::cout << "Value read is saved in the file " << value
                          << std::endl;
                return;
            }
            else
            {
                std::cerr << "Error while saving the read value in file. "
                             "Displaying the read value on console"
                          << std::endl;
            }
        }

        json output = json::object({});
        json kwVal = json::object({});
        kwVal.emplace(keyword, printableVal);

        output.emplace(fruPath, kwVal);

        debugger(output);
    }
    catch (const json::exception& e)
    {
        std::cout << "Keyword Value: " << keyword << std::endl;
        std::cout << e.what() << std::endl;
    }
}

int VpdTool::updateKeyword()
{
    Binary val;

    if (std::filesystem::exists(value))
    {
        if (!fileToVector(val))
        {
            std::cout << "Keyword " << keyword << " update failed."
                      << std::endl;
            return 1;
        }
    }
    else
    {
        val = toBinary(value);
    }

    auto bus = sdbusplus::bus::new_default();
    auto properties = bus.new_method_call(BUSNAME, OBJPATH, IFACE,
                                          "WriteKeyword");
    properties.append(static_cast<sdbusplus::message::object_path>(fruPath));
    properties.append(recordName);
    properties.append(keyword);
    properties.append(val);

    // When there is a request to write 10K bytes, there occurs a delay in dbus
    // call which leads to dbus timeout exception. To avoid such exceptions
    // increase the timeout period from default 25 seconds to 60 seconds.
    auto timeoutInMicroSeconds = 60 * 1000000L;
    auto result = bus.call(properties, timeoutInMicroSeconds);

    if (result.is_method_error())
    {
        throw runtime_error("Get api failed");
    }
    std::cout << "Data updated successfully " << std::endl;
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
            {}
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
    Binary val;
    if (std::filesystem::exists(value))
    {
        if (!fileToVector(val))
        {
            std::cout << "Keyword " << keyword << " update failed."
                      << std::endl;
            return 1;
        }
    }
    else
    {
        val = toBinary(value);
    }

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
    std::cout << "Data updated successfully " << std::endl;
    return rc;
}

void VpdTool::readKwFromHw(const uint32_t& startOffset)
{
    ifstream inventoryJson(INVENTORY_JSON_SYM_LINK);
    auto jsonFile = nlohmann::json::parse(inventoryJson);
    std::string inventoryPath;

    if (jsonFile["frus"].contains(fruPath))
    {
        uint32_t vpdStartOffset = 0;

        for (const auto& item : jsonFile["frus"][fruPath])
        {
            if (item.find("offset") != item.end())
            {
                vpdStartOffset = item["offset"];
                break;
            }
        }

        if ((startOffset != vpdStartOffset))
        {
            std::cerr << "Invalid offset, please correct the offset" << endl;
            std::cerr << "Recommended Offset is: " << vpdStartOffset << endl;
            return;
        }
        inventoryPath = jsonFile["frus"][fruPath][0]["inventoryPath"];
    }

    Binary completeVPDFile;
    fstream vpdFileStream;

    vpdFileStream.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    try
    {
        vpdFileStream.open(fruPath,
                           std::ios::in | std::ios::out | std::ios::binary);

        auto vpdFileSize = std::min(std::filesystem::file_size(fruPath),
                                    constants::MAX_VPD_SIZE);
        if (vpdFileSize == 0)
        {
            std::cerr << "File size is 0 for " << fruPath << std::endl;
            throw std::runtime_error("File size is 0.");
        }

        completeVPDFile.resize(vpdFileSize);
        vpdFileStream.seekg(startOffset, ios_base::cur);
        vpdFileStream.read(reinterpret_cast<char*>(&completeVPDFile[0]),
                           vpdFileSize);
        vpdFileStream.clear(std::ios_base::eofbit);
    }
    catch (const std::system_error& fail)
    {
        std::cerr << "Exception in file handling [" << fruPath
                  << "] error : " << fail.what();
        std::cerr << "Stream file size = " << vpdFileStream.gcount()
                  << std::endl;
        throw;
    }

    if (completeVPDFile.empty())
    {
        throw std::runtime_error("Invalid File");
    }

    Impl obj(completeVPDFile, (constants::pimPath + inventoryPath), fruPath,
             startOffset);
    std::string keywordVal = obj.readKwFromHw(recordName, keyword);

    keywordVal = getPrintableValue(keywordVal);

    if (keywordVal.empty())
    {
        std::cerr << "The given keyword " << keyword << " or record "
                  << recordName
                  << " or both are not present in the given FRU path "
                  << fruPath << std::endl;
        return;
    }

    if (!value.empty())
    {
        if (copyStringToFile(keywordVal))
        {
            std::cout << "Value read is saved in the file " << value
                      << std::endl;
            return;
        }
        else
        {
            std::cerr
                << "Error while saving the read value in file. Displaying "
                   "the read value on console"
                << std::endl;
        }
    }

    json output = json::object({});
    json kwVal = json::object({});
    kwVal.emplace(keyword, keywordVal);
    output.emplace(fruPath, kwVal);
    debugger(output);
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

    uint8_t num = 0;

    // Get system VPD data in map
    unordered_map<string, DbusPropertyMap> vpdMap;
    json js;
    getVPDInMap(constants::systemVpdFilePath, vpdMap, js,
                constants::pimPath +
                    static_cast<std::string>(constants::SYSTEM_OBJECT));

    // Get system VPD D-Bus Data in a map
    IntfPropMap svpdBusData;
    getSystemDataFromCache(svpdBusData);

    for (const auto& recordKw : svpdKwdMap)
    {
        string record = recordKw.first;

        // Extract specific record data from the svpdBusData map.
        const auto& rec = svpdBusData.find(record);

        if (rec == svpdBusData.end())
        {
            std::cerr << record << " not a part of critical system VPD records."
                      << std::endl;
            continue;
        }

        const auto& recData = svpdBusData.find(record)->second;

        string busStr{}, hwValStr{};

        for (const auto& keywordInfo : recordKw.second)
        {
            const auto& keyword = get<0>(keywordInfo);
            string mismatch = "NO"; // no mismatch
            string hardwareValue{};
            auto recItr = vpdMap.find(record);

            if (recItr != vpdMap.end())
            {
                DbusPropertyMap& kwValMap = recItr->second;
                auto kwItr = kwValMap.find(keyword);
                if (kwItr != kwValMap.end())
                {
                    hardwareValue = kwItr->second;
                }
            }

            inventory::Value kwValue;
            for (auto& kwData : recData)
            {
                if (kwData.first == keyword)
                {
                    kwValue = kwData.second;
                    break;
                }
            }

            if (keyword != "SE") // SE to display in Hex string only
            {
                ostringstream hwValStream;
                hwValStream << "0x";
                hwValStr = hwValStream.str();

                for (uint16_t byte : hardwareValue)
                {
                    hwValStream << setfill('0') << setw(2) << hex << byte;
                    hwValStr = hwValStream.str();
                }

                if (const auto value = get_if<Binary>(&kwValue))
                {
                    busStr = hexString(*value);
                }
                if (busStr != hwValStr)
                {
                    mismatch = "YES";
                }
            }
            else
            {
                if (const auto value = get_if<Binary>(&kwValue))
                {
                    busStr = getPrintableValue(*value);
                }
                if (busStr != hardwareValue)
                {
                    mismatch = "YES";
                }
                hwValStr = hardwareValue;
            }
            recKwData.push_back(
                make_tuple(++num, record, keyword, busStr, hwValStr, mismatch));

            std::string splitLine(191, '-');
            cout << left << setw(6) << static_cast<int>(num) << left << setw(8)
                 << record << left << setw(9) << keyword << left << setw(75)
                 << setfill(' ') << busStr << left << setw(75) << setfill(' ')
                 << hwValStr << left << setw(14) << mismatch << '\n'
                 << splitLine << endl;
        }
    }
    parseSVPDOptions(js, std::string());
    return 0;
}

void VpdTool::parseSVPDOptions(const nlohmann::json& json,
                               const std::string& backupEEPROMPath)
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
            std::string hardwarePath = constants::systemVpdFilePath;
            if (!backupEEPROMPath.empty())
            {
                hardwarePath = backupEEPROMPath;
            }

            for (const auto& data : recKwData)
            {
                if (get<5>(data) == "YES")
                {
                    std::string record = get<1>(data), keyword = get<2>(data);

                    if (!backupEEPROMPath.empty())
                    {
                        getBackupRecordKeyword(record, keyword);
                    }

                    EditorImpl edit(hardwarePath, json, record, keyword);
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

                    cout << left << setw(6) << static_cast<int>(get<0>(data))
                         << left << setw(8) << get<1>(data) << left << setw(9)
                         << get<2>(data) << left << setw(75) << setfill(' ')
                         << get<3>(data) << left << setw(75) << setfill(' ')
                         << get<4>(data) << left << setw(14) << get<5>(data);

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

                    if (option == VpdTool::BACKUP_DATA_FOR_CURRENT)
                    {
                        EditorImpl edit(constants::systemVpdFilePath, json,
                                        get<1>(data), get<2>(data));
                        edit.updateKeyword(toBinary(get<3>(data)), 0, true);
                        cout << "\nData updated successfully.\n";
                        break;
                    }
                    else if (option ==
                             VpdTool::SYSTEM_BACKPLANE_DATA_FOR_CURRENT)
                    {
                        std::string hardwarePath = constants::systemVpdFilePath;
                        std::string record = get<1>(data);
                        std::string keyword = get<2>(data);

                        if (!backupEEPROMPath.empty())
                        {
                            hardwarePath = backupEEPROMPath;
                            getBackupRecordKeyword(record, keyword);
                        }

                        EditorImpl edit(hardwarePath, json, record, keyword);
                        edit.updateKeyword(toBinary(get<4>(data)), 0, true);
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

                        EditorImpl edit(constants::systemVpdFilePath, json,
                                        get<1>(data), get<2>(data));
                        edit.updateKeyword(toBinary(value), 0, true);

                        if (!backupEEPROMPath.empty())
                        {
                            std::string record = get<1>(data);
                            std::string keyword = get<2>(data);

                            getBackupRecordKeyword(record, keyword);
                            EditorImpl edit(backupEEPROMPath, json, record,
                                            keyword);
                            edit.updateKeyword(toBinary(value), 0, true);
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
                }
            }
        }

        std::cout << "\n The critical keywords from system backplane VPD has "
                     "been reset successfully."
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

int VpdTool::fixSystemBackupVPD(const std::string& backupEepromPath,
                                const std::string& backupInvPath)
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

    uint8_t num = 0;
    // Get system VPD data in map
    unordered_map<string, DbusPropertyMap> systemVPDMap;
    json js;
    getVPDInMap(constants::systemVpdFilePath, systemVPDMap, js,
                constants::pimPath +
                    static_cast<std::string>(constants::SYSTEM_OBJECT));

    // Get backup VPD data in map
    unordered_map<string, DbusPropertyMap> backupVPDMap;
    getVPDInMap(backupEepromPath, backupVPDMap, js,
                constants::pimPath + backupInvPath);

    for (const auto& recordKw : svpdKwdMap)
    {
        const std::string& primaryRecord = recordKw.first;

        std::string primaryValStr{}, backupValStr{};

        for (const auto& keywordInfo : recordKw.second)
        {
            const auto& primaryKeyword = get<0>(keywordInfo);
            const auto& bkRecord = get<4>(keywordInfo);
            const auto& bkKeyword = get<5>(keywordInfo);
            string mismatch = "NO";
            string primaryValue{};
            string backupValue{};

            // Find keyword value for system VPD (primary VPD)
            auto primaryRecItr = systemVPDMap.find(primaryRecord);
            if (primaryRecItr != systemVPDMap.end())
            {
                DbusPropertyMap& primaryKwValMap = primaryRecItr->second;
                auto kwItr = primaryKwValMap.find(primaryKeyword);
                if (kwItr != primaryKwValMap.end())
                {
                    primaryValue = kwItr->second;
                }
            }

            // Find keyword value for backup VPD
            auto bkRecItr = backupVPDMap.find(bkRecord);
            if (bkRecItr != backupVPDMap.end())
            {
                DbusPropertyMap& bkKwValMap = bkRecItr->second;
                auto kwItr = bkKwValMap.find(bkKeyword);
                if (kwItr != bkKwValMap.end())
                {
                    backupValue = kwItr->second;
                }
            }

            // SE to display in hex string only
            if (primaryKeyword != "SE")
            {
                ostringstream hwValStream;
                hwValStream << "0x";
                primaryValStr = hwValStream.str();

                for (uint16_t byte : primaryValue)
                {
                    hwValStream << setfill('0') << setw(2) << hex << byte;
                    primaryValStr = hwValStream.str();
                }

                hwValStream.str(std::string());
                hwValStream << "0x";
                backupValStr = hwValStream.str();

                for (uint16_t byte : backupValue)
                {
                    hwValStream << setfill('0') << setw(2) << hex << byte;
                    backupValStr = hwValStream.str();
                }
                if (primaryValStr != backupValStr)
                {
                    mismatch = "YES";
                }
            }
            else
            {
                if (primaryValue != backupValue)
                {
                    mismatch = "YES";
                }

                primaryValStr = primaryValue;
                backupValStr = backupValue;
            }

            recKwData.push_back(make_tuple(++num, primaryRecord, primaryKeyword,
                                           backupValStr, primaryValStr,
                                           mismatch));

            std::string splitLine(191, '-');
            cout << left << setw(6) << static_cast<int>(num) << left << setw(8)
                 << primaryRecord << left << setw(9) << primaryKeyword << left
                 << setw(75) << setfill(' ') << backupValStr << left << setw(75)
                 << setfill(' ') << primaryValStr << left << setw(14)
                 << mismatch << '\n'
                 << splitLine << endl;
        }
    }

    parseSVPDOptions(js, backupEepromPath);
    return 0;
}
