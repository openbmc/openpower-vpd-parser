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
    static std::shared_ptr<sdbusplus::bus::match_t> nameOwnerMatch =
        std::make_shared<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::nameOwnerChanged(
                "xyz.openbmc_project.PLDM"),
            [this](sdbusplus::message_t& msg) {
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
    static std::shared_ptr<sdbusplus::bus::match_t> biosMatcher =
        std::make_shared<sdbusplus::bus::match_t>(
            bus,
            sdbusplus::bus::match::rules::propertiesChanged(
                "/xyz/openbmc_project/bios_config/manager",
                "xyz.openbmc_project.BIOSConfig.Manager"),
            [this](sdbusplus::message_t& msg) { biosAttribsCallback(msg); });
}

void BiosHandler::biosAttribsCallback(sdbusplus::message_t& msg)
{
    if (msg.is_method_error())
    {
        std::cerr << "Error in reading BIOS attribute signal " << std::endl;
        return;
    }

    std::string object;
    BiosBaseTableType propMap;
    msg.read(object, propMap);
    for (auto prop : propMap)
    {
        if (prop.first == "BaseBIOSTable")
        {
            auto list = std::get<0>(prop.second);
            for (const auto& item : list)
            {
                std::string attributeName = std::get<0>(item);
                auto attrValue = std::get<5>(std::get<1>(item));

                auto attrData = attributeTable.find(attributeName);
                if (attrData == attributeTable.end())
                {
                    continue;
                }

                auto valueFromVPD = readBusProperty(
                    SYSTEM_OBJECT, "com.ibm.ipzvpd." + get<0>(attrData->second),
                    get<1>(attrData->second));

                if (auto val = std::get_if<std::string>(&attrValue))
                {
                    saveBiosAttrToVPD(attributeName, *val, valueFromVPD);
                }
                else if (auto val = std::get_if<int64_t>(&attrValue))
                {
                    saveBiosAttrToVPD(attributeName, *val, valueFromVPD);
                }
            }
        }
    }
}

void BiosHandler::saveBiosAttrToVPD(const BIOSAttribute& attrName,
                                    const BIOSAttrValueType& attrValInBios,
                                    const std::string& attrValInVPD)
{
    Binary vpdNewVal;

    auto attrData = attributeTable.find(attrName);
    if (attrData == attributeTable.end())
    {
        return;
    }

    auto record = get<0>(attrData->second);
    auto keyword = get<1>(attrData->second);

    if (auto val = get_if<int64_t>(&attrValInBios))
    {
        if (*val == -1)
        {
            std::cerr << "Invalid attribute's value from BIOS- [ " << attrName
                      << " : " << *val << " ]" << std::endl;
            return;
        }

        vpdNewVal = {0, 0, 0, static_cast<uint8_t>(*val)};

        if (attrValInVPD.size() != 4)
        {
            std::cerr << "Read bad size from VPD for the record keyword pair "
                      << record << " " << keyword << " for the attribute "
                      << attrName << ". Size read = " << attrValInVPD.size()
                      << std::endl;
            return;
        }

        if (attrValInVPD.at(3) == *val)
        {
            std::cout << "Skip updating " << attrName
                      << " to VPD. No mismatch found. Value = " << *val
                      << std::endl;
            return;
        }
    }
    else if (auto attrBiosValue = get_if<std::string>(&attrValInBios))
    {
        if (attrValInVPD.size() != 1)
        {
            std::cerr << "Read bad size from VPD for the record keyword pair "
                      << record << " " << keyword << " for the attribute "
                      << attrName << ". Size read = " << attrValInVPD.size()
                      << std::endl;
            return;
        }

        auto bitmask = get<2>(attrData->second);

        // Update corresponding record keyword on VPD, if there is a value
        // mismatch in BIOS and VPD.
        if (*attrBiosValue == "Enabled")
        {
            if ((attrName == "hb_memory_mirror_mode") &&
                (attrValInVPD.at(0) != 2))
            {
                vpdNewVal.emplace_back(2);
            }
            else if ((attrValInVPD.at(0) & bitmask) != bitmask)
            {
                vpdNewVal.emplace_back(attrValInVPD.at(0) | bitmask);
            }
        }
        else if (*attrBiosValue == "Disabled")
        {
            if ((attrName == "hb_memory_mirror_mode") &&
                attrValInVPD.at(0) != 1)
            {
                vpdNewVal.emplace_back(1);
            }
            else if ((attrValInVPD.at(0) & bitmask) != 0)
            {
                vpdNewVal.emplace_back(attrValInVPD.at(0) & ~bitmask);
            }
        }
        else
        {
            std::cerr << "Read bad " << attrName
                      << " value from BIOS. Value = " << *attrBiosValue
                      << std::endl;
            return;
        }

        if (vpdNewVal.empty())
        {
            std::cout << "Skip updating " << attrName << "  to VPD "
                      << std::endl;
            return;
        }
    }

    manager.writeKeyword(sdbusplus::message::object_path{SYSTEM_OBJECT}, record,
                         keyword, vpdNewVal);
    std::cout << "\nUpdated " << attrName << "  to VPD with the value = "
              << static_cast<int>(vpdNewVal.at(0)) << std::endl;
}

void BiosHandler::saveAttrToBIOS(const BIOSAttribute& attrName,
                                 const std::string& attrVpdVal,
                                 const BIOSAttrValueType& attrInBIOS)
{
    PendingBIOSAttrsType biosAttrs;

    auto attrData = attributeTable.find(attrName);
    if (attrData == attributeTable.end())
    {
        return;
    }

    auto record = get<0>(attrData->second);
    auto keyword = get<1>(attrData->second);

    if (auto pVal = get_if<int64_t>(&attrInBIOS))
    {
        if (attrVpdVal.size() != 4)
        {
            std::cerr << "Read bad size from VPD for the record keyword pair "
                      << record << " " << keyword << " for the attribute "
                      << attrName << ". Size read = " << attrVpdVal.size()
                      << std::endl;
            return;
        }

        if (*pVal == static_cast<int64_t>(attrVpdVal.at(3)))
        {
            std::cout << "Skip updating " << attrName
                      << " to BIOS. No mismatch found. Value = " << *pVal
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
    else if (auto attrBiosVal = get_if<std::string>(&attrInBIOS))
    {
        if (attrVpdVal.size() != 1)
        {
            std::cerr << "Read bad size from VPD for the record keyword pair "
                      << record << " " << keyword << " for the attribute "
                      << attrName << ". Size read = " << attrVpdVal.size()
                      << std::endl;
            return;
        }

        auto bitmask = get<2>(attrData->second);

        std::string toWrite;
        if (attrName == "hb_memory_mirror_mode")
        {
            if (attrVpdVal.at(0) != 1 && attrVpdVal.at(0) != 2)
            {
                std::cerr << "Read bad " << attrName
                          << " value from VPD. Value = "
                          << static_cast<int>(attrVpdVal.at(0)) << std::endl;
                return;
            }

            toWrite = (attrVpdVal.at(0) == 2) ? "Enabled" : "Disabled";
        }
        else
        {
            toWrite = (attrVpdVal.at(0) & bitmask) ? "Enabled" : "Disabled";
        }

        if (*attrBiosVal == toWrite)
        {
            std::cout << "Skip updating " << attrName
                      << " to BIOS. No mismatch found. Value = " << toWrite
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
        auto attributeName = biosAttrMap.first;

        auto valFromVPD = readBusProperty(
            SYSTEM_OBJECT, "com.ibm.ipzvpd." + get<0>(biosAttrMap.second),
            get<1>(biosAttrMap.second));

        auto valFromBios = readBIOSAttribute(attributeName);

        if (auto pVal = get_if<int64_t>(&valFromBios))
        {
            if (valFromVPD.at(0) == 0)
            {
                saveBiosAttrToVPD(attributeName, *pVal, valFromVPD);
            }
            else
            {
                saveAttrToBIOS(attributeName, valFromVPD, *pVal);
            }
        }
        else if (auto pVal = get_if<std::string>(&valFromBios))
        {
            if (valFromVPD == "    ")
            {
                saveBiosAttrToVPD(attributeName, *pVal, valFromVPD);
            }
            else
            {
                saveAttrToBIOS(attributeName, valFromVPD, *pVal);
            }
        }
    }

    // Start listener now that we have done the restore
    listenBiosAttribs();
}
} // namespace manager
} // namespace vpd
} // namespace openpower
