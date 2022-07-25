#include "config.h"

#include "bios_handler.hpp"

#include "const.hpp"
#include "ibm_vpd_utils.hpp"
#include "manager.hpp"
#include "types.hpp"

#include <iostream>
#include <memory>
#include <sdbusplus/bus.hpp>
#include <string>
#include <tuple>
#include <variant>

using namespace openpower::vpd;
using namespace openpower::vpd::constants;

namespace openpower
{
namespace vpd
{
namespace manager
{
void BiosHandler::checkAndListenPLDMService()
{
    // Setup a match on NameOwnerChanged to determine when PLDM is up. In
    // the signal handler, call restoreBIOSAttribs
    static std::shared_ptr<sdbusplus::bus::match::match> nameOwnerMatch =
        std::make_shared<sdbusplus::bus::match::match>(
            bus,
            sdbusplus::bus::match::rules::nameOwnerChanged(
                "xyz.openbmc_project.PLDM"),
            [this](sdbusplus::message::message& msg) {
                if (msg.is_method_error())
                {
                    std::cerr << "Error in reading name owner signal "
                              << std::endl;
                    return;
                }
                std::string name;
                std::string newOwner;
                std::string oldOwner;

                msg.read(name, oldOwner, newOwner);
                if (newOwner != "" && name == "xyz.openbmc_project.PLDM")
                {
                    this->restoreBIOSAttribs();
                    // We don't need the match anymore
                    nameOwnerMatch.reset();
                }
            });
    // Check if PLDM is already running, if it is, we can go ahead and attempt
    // to sync BIOS attributes (since PLDM would have initialized them by the
    // time it acquires a bus name).
    bool isPLDMRunning = false;
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                "org.freedesktop.DBus", "NameHasOwner");
        method.append("xyz.openbmc_project.PLDM");

        auto result = bus.call(method);
        result.read(isPLDMRunning);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        std::cerr << "Failed to check if PLDM is running, assume false"
                  << std::endl;
        std::cerr << e.what() << std::endl;
    }
    std::cout << "Is PLDM running: " << isPLDMRunning << std::endl;
    if (isPLDMRunning)
    {
        nameOwnerMatch.reset();
        restoreBIOSAttribs();
    }
}

void BiosHandler::listenBiosAttribs()
{
    static std::shared_ptr<sdbusplus::bus::match::match> biosMatcher =
        std::make_shared<sdbusplus::bus::match::match>(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                "/xyz/openbmc_project/bios_config/manager",
                "xyz.openbmc_project.BIOSConfig.Manager"),
            [this](sdbusplus::message::message& msg) {
                biosAttribsCallback(msg);
            });
}

biosAttrTable attributeTable = {
    {"hb_field_core_override", make_tuple("int64_t", "VSYS", "RG")},
    {"hb_memory_mirror_mode", make_tuple("string", "UTIL", "D0")},
    {"pvm_keep_and_clear", make_tuple("string", "UTIL", "D1")},
    {"pvm_create_default_lpar", make_tuple("string", "UTIL", "D1")},
    {"pvm_clear_nvram", make_tuple("string", "UTIL", "D1")}};

attributeValues attrValuesSet;

void BiosHandler::biosAttribsCallback(sdbusplus::message::message& msg)
{
    if (msg.is_method_error())
    {
        std::cerr << "Error in reading BIOS attribute signal " << std::endl;
        return;
    }
    using BiosProperty = std::tuple<
        std::string, bool, std::string, std::string, std::string,
        std::variant<int64_t, std::string>, std::variant<int64_t, std::string>,
        std::vector<
            std::tuple<std::string, std::variant<int64_t, std::string>>>>;
    using BiosBaseTable =
        std::variant<std::map<std::string, BiosProperty>>; // TODO check why
                                                           // variant?
    using BiosBaseTableType = std::map<std::string, BiosBaseTable>;

    std::string object;
    BiosBaseTableType propMap;
    msg.read(object, propMap);

    // Update the attribute bios-vpd map
    for (auto& biosAttrMap : attributeTable)
    {
        auto val = readBusProperty(
            SYSTEM_OBJECT, "com.ibm.ipzvpd." + get<1>(biosAttrMap.second),
            get<2>(biosAttrMap.second));

        attrValuesSet[biosAttrMap.first].second = val;
    }

    for (auto prop : propMap)
    {
        if (prop.first == "BaseBIOSTable")
        {
            auto list = std::get<0>(prop.second);
            for (const auto& item : list)
            {
                std::string attributeName = std::get<0>(item);
                auto attrValue = std::get<5>(std::get<1>(item));

                if ((attributeName == "hb_memory_mirror_mode") ||
                    (attributeName == "pvm_keep_and_clear") ||
                    (attributeName == "pvm_create_default_lpar") ||
                    (attributeName == "pvm_clear_nvram"))
                {
                    auto val = std::get_if<std::string>(&attrValue);
                    if (val)
                    {
                        saveBiosAttrToVPD(attributeName, *val,
                                          attrValuesSet[attributeName].second);
                    }
                }
                else if (attributeName == "hb_field_core_override")
                {
                    auto val = std::get_if<int64_t>(&attrValue);
                    if (val)
                    {
                        saveBiosAttrToVPD(attributeName, *val,
                                          attrValuesSet[attributeName].second);
                    }
                }
            }
        }
    }
}

void BiosHandler::saveBiosAttrToVPD(const string& attrName,
                                    const biosAttrValue& attrValInBios,
                                    const string& attrValInVPD)
{
    Binary vpdNewVal;
    if (auto fcoVal = get_if<int64_t>(&attrValInBios))
    {
        if (*fcoVal == -1)
        {
            std::cerr << "Invalid attribute's value from BIOS- [ " << attrName
                      << " : " << *fcoVal << " ]" << std::endl;
            return;
        }

        vpdNewVal = {0, 0, 0, static_cast<uint8_t>(*fcoVal)};

        if (attrValInVPD.size() != 4)
        {
            std::cerr << "Read bad size for VSYS/RG: " << attrValInVPD.size()
                      << std::endl;
            return;
        }

        if (attrValInVPD.at(3) == *fcoVal)
        {
            std::cout << "Skip Updating FCO to VPD,it has same value "
                      << *fcoVal << std::endl;
            return;
        }
    }
    else if (auto attrBiosValue = get_if<string>(&attrValInBios))
    {
        if (attrValInVPD.size() != 1)
        {
            std::cerr << "bad size of vpd value for : [" << attrName << " : "
                      << attrValInVPD.size() << " ]" << std::endl;
            return;
        }

        if (*attrBiosValue != "Enabled" && *attrBiosValue != "Disabled")
        {
            std::cerr << "Bad value for BIOS attribute: [" << attrName << " : "
                      << *attrBiosValue << " ]" << std::endl;
            return;
        }

        // Write to VPD only if the value is not already what we want to write.
        if (*attrBiosValue == "Enabled")
        {
            if ((attrName == "hb_memory_mirror_mode") &&
                (attrValInVPD.at(0) != 2))
            {
                vpdNewVal.emplace_back(2);
            }
            else if ((attrName == "pvm_keep_and_clear") &&
                     ((attrValInVPD.at(0) & 0x01) != 0x01))
            {
                vpdNewVal.emplace_back(attrValInVPD.at(0) | 0x01);
            }
            else if ((attrName == "pvm_create_default_lpar") &&
                     ((attrValInVPD.at(0) & 0x02) != 0x02))
            {
                vpdNewVal.emplace_back(attrValInVPD.at(0) | 0x02);
            }
            else if ((attrName == "pvm_clear_nvram") &&
                     ((attrValInVPD.at(0) & 0x04) != 0x04))
            {
                vpdNewVal.emplace_back(attrValInVPD.at(0) | 0x04);
            }
        }
        else if (*attrBiosValue == "Disabled")
        {
            if ((attrName == "hb_memory_mirror_mode") &&
                attrValInVPD.at(0) != 1)
            {
                vpdNewVal.emplace_back(1);
            }

            else if ((attrName == "pvm_keep_and_clear") &&
                     ((attrValInVPD.at(0) & 0x01) != 0))
            {
                vpdNewVal.emplace_back(attrValInVPD.at(0) & ~(0x01));
            }

            else if ((attrName == "pvm_create_default_lpar") &&
                     ((attrValInVPD.at(0) & 0x02) != 0))
            {
                vpdNewVal.emplace_back(attrValInVPD.at(0) & ~(0x02));
            }

            else if ((attrName == "pvm_clear_nvram") &&
                     ((attrValInVPD.at(0) & 0x04) != 0))
            {
                vpdNewVal.emplace_back(attrValInVPD.at(0) & ~(0x04));
            }
        }

        if (vpdNewVal.empty())
        {
            std::cout << "Skip Updating " << attrName << "  to VPD "
                      << std::endl;
            return;
        }
    }
    std::cout << "Updating " << attrName
              << "  to VPD: " << static_cast<int>(vpdNewVal.at(0)) << std::endl;
    manager.writeKeyword(sdbusplus::message::object_path{SYSTEM_OBJECT},
                         get<1>(attributeTable[attrName]),
                         get<2>(attributeTable[attrName]), vpdNewVal);
}

void BiosHandler::saveAttrToBIOS(const std::string& attrName,
                                 const std::string& attrVpdVal,
                                 const biosAttrValue& attrInBIOS)
{
    PendingBIOSAttrsType biosAttrs;
    if (auto pVal = get_if<int64_t>(&attrInBIOS))
    {
        auto fcoBiosVal = *pVal;
        if (attrVpdVal.size() != 4)
        {
            std::cerr << "Bad size for FCO in VPD: " << attrVpdVal.size()
                      << std::endl;
            return;
        }

        // Need to write?
        if (fcoBiosVal == static_cast<int64_t>(attrVpdVal.at(3)))
        {
            std::cout << "Skip FCO BIOS write, value is already: " << fcoBiosVal
                      << std::endl;
            return;
        }
        biosAttrs.push_back(std::make_pair(
            attrName, std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                      "AttributeType.Integer",
                                      attrVpdVal.at(3))));

        std::cout << "Set " << attrName << " to: " << attrVpdVal.at(3)
                  << std::endl;
    }
    else if (auto attrBiosVal = get_if<string>(&attrInBIOS))
    {
        if (attrVpdVal.size() != 1)
        {
            std::cerr << "Bad size for attribute[" << attrName
                      << "] in VPD: " << attrVpdVal.size() << std::endl;
            return;
        }

        std::string toWrite;
        // Make sure data in VPD is sane
        if (attrName == "hb_memory_mirror_mode")
        {
            if (attrVpdVal.at(0) != 1 && attrVpdVal.at(0) != 2)
            {
                std::cerr << "Bad value for AMM read from VPD: "
                          << static_cast<int>(attrVpdVal.at(0)) << std::endl;
                return;
            }

            toWrite = (attrVpdVal.at(0) == 2) ? "Enabled" : "Disabled";
        }
        else if (attrName == "pvm_clear_nvram")
        {
            toWrite = (attrVpdVal.at(0) & 0x04) ? "Enabled" : "Disabled";
        }
        else if (attrName == "pvm_create_default_lpar")
        {
            toWrite = (attrVpdVal.at(0) & 0x02) ? "Enabled" : "Disabled";
        }
        else if (attrName == "pvm_keep_and_clear")
        {
            toWrite = (attrVpdVal.at(0) & 0x01) ? "Enabled" : "Disabled";
        }

        if (*attrBiosVal == toWrite)
        {
            std::cout << "Skip BIOS write for- " << attrName
                      << ", value is already updated - " << toWrite
                      << std::endl;
            return;
        }
        biosAttrs.push_back(std::make_pair(
            attrName, std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                      "AttributeType.Enumeration",
                                      toWrite)));

        std::cout << "Set " << attrName << " to: " << toWrite << std::endl;
    }

    setBusProperty<PendingBIOSAttrsType>(
        "xyz.openbmc_project.BIOSConfigManager",
        "/xyz/openbmc_project/bios_config/manager",
        "xyz.openbmc_project.BIOSConfig.Manager", "PendingAttributes",
        biosAttrs);
}

void BiosHandler::restoreBIOSAttribs()
{
    std::cout << "Attempting BIOS attribute reset" << std::endl;
    // Check if the VPD contains valid data for FCO, AMM, Keep and Clear,
    // Create default LPAR and Clear NVRAM *and* that it differs from the data
    // already in the attributes. If so, set the BIOS attributes as per the
    // value in the VPD. If the VPD contains default data, then initialize the
    // VPD keywords with data taken from the BIOS.

    for (auto& biosAttrMap : attributeTable)
    {
        auto val = readBusProperty(
            SYSTEM_OBJECT, "com.ibm.ipzvpd." + get<1>(biosAttrMap.second),
            get<2>(biosAttrMap.second));

        attrValuesSet[biosAttrMap.first].second = val;
    }

    for (auto& biosAttrMap : attributeTable)
    {
        auto val = readBIOSAttribute(biosAttrMap.first);

        if (auto pVal = get_if<int64_t>(&val))
        {
            attrValuesSet[biosAttrMap.first].first = *pVal;
        }
        else if (auto pVal = get_if<string>(&val))
        {
            attrValuesSet[biosAttrMap.first].first = *pVal;
        }
    }

    for (auto& biosAttrMap : attributeTable)
    {
        // No uninitialized handling needed for keep and clear, create default
        // lpar and clear nvram attributes. Their defaults in VPD are 0's which
        // is what we want.

        if ("string" == (get<0>(biosAttrMap.second)))
        {
            auto pVal = get_if<string>(&attrValuesSet[biosAttrMap.first].first);
            if (attrValuesSet[biosAttrMap.first].second == "    ")
            {
                if (pVal)
                    saveBiosAttrToVPD(biosAttrMap.first, *pVal,
                                      attrValuesSet[biosAttrMap.first].second);
            }
            else
            {
                if (pVal)
                    saveAttrToBIOS(biosAttrMap.first,
                                   attrValuesSet[biosAttrMap.first].second,
                                   *pVal);
            }
        }

        else if ("int64_t" == (get<0>(biosAttrMap.second)))
        {
            auto pVal =
                get_if<int64_t>(&attrValuesSet[biosAttrMap.first].first);
            if (attrValuesSet[biosAttrMap.first].second.at(0) == 0)
            {
                if (pVal)
                    saveBiosAttrToVPD(biosAttrMap.first, *pVal,
                                      attrValuesSet[biosAttrMap.first].second);
            }
            else
            {
                if (pVal)
                    saveAttrToBIOS(biosAttrMap.first,
                                   attrValuesSet[biosAttrMap.first].second,
                                   *pVal);
            }
        }
    }

    // Start listener now that we have done the restore
    listenBiosAttribs();
}
} // namespace manager
} // namespace vpd
} // namespace openpower