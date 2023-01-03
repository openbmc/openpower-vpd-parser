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
            *conn,
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
            *conn,
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
    using BiosProperty = std::tuple<
        std::string, bool, std::string, std::string, std::string,
        std::variant<int64_t, std::string>, std::variant<int64_t, std::string>,
        std::vector<
            std::tuple<std::string, std::variant<int64_t, std::string>>>>;
    using BiosBaseTable = std::variant<std::map<std::string, BiosProperty>>;
    using BiosBaseTableType = std::map<std::string, BiosBaseTable>;

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
                if (attributeName == "hb_memory_mirror_mode")
                {
                    auto attrValue = std::get<5>(std::get<1>(item));
                    auto val = std::get_if<std::string>(&attrValue);
                    if (val)
                    {
                        saveAMMToVPD(*val);
                    }
                }
                else if (attributeName == "hb_field_core_override")
                {
                    auto attrValue = std::get<5>(std::get<1>(item));
                    auto val = std::get_if<int64_t>(&attrValue);
                    if (val)
                    {
                        saveFCOToVPD(*val);
                    }
                }
                else if (attributeName == "pvm_keep_and_clear")
                {
                    auto attrValue = std::get<5>(std::get<1>(item));
                    auto val = std::get_if<std::string>(&attrValue);
                    if (val)
                    {
                        saveKeepAndClearToVPD(*val);
                    }
                }
                else if (attributeName == "pvm_create_default_lpar")
                {
                    auto attrValue = std::get<5>(std::get<1>(item));
                    auto val = std::get_if<std::string>(&attrValue);
                    if (val)
                    {
                        saveCreateDefaultLparToVPD(*val);
                    }
                }
                else if (attributeName == "pvm_clear_nvram")
                {
                    auto attrValue = std::get<5>(std::get<1>(item));
                    auto val = std::get_if<std::string>(&attrValue);
                    if (val)
                    {
                        saveClearNVRAMToVPD(*val);
                    }
                }
            }
        }
    }
}

void BiosHandler::saveFCOToVPD(int64_t fcoVal)
{
    if (fcoVal == -1)
    {
        std::cerr << "Invalid FCO value from BIOS: " << fcoVal << std::endl;
        return;
    }

    types::Binary vpdVal = {0, 0, 0, static_cast<uint8_t>(fcoVal)};
    auto valInVPD = readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VSYS", "RG");

    if (valInVPD.size() != 4)
    {
        std::cerr << "Read bad size for VSYS/RG: " << valInVPD.size()
                  << std::endl;
        return;
    }

    if (std::memcmp(vpdVal.data(), valInVPD.data(), 4) != 0)
    {
        std::cout << "Writing FCO to VPD: " << fcoVal << std::endl;
        manager.writeKeyword(sdbusplus::message::object_path{SYSTEM_OBJECT},
                             "VSYS", "RG", vpdVal);
    }
}

void BiosHandler::saveAMMToVPD(const std::string& mirrorMode)
{
    types::Binary vpdVal;
    auto valInVPD = readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.UTIL", "D0");

    if (valInVPD.size() != 1)
    {
        std::cerr << "Read bad size for UTIL/D0: " << valInVPD.size()
                  << std::endl;
        return;
    }

    if (mirrorMode != "Enabled" && mirrorMode != "Disabled")
    {
        std::cerr << "Bad value for Mirror mode BIOS attribute: " << mirrorMode
                  << std::endl;
        return;
    }

    // Write to VPD only if the value is not already what we want to write.
    if (mirrorMode == "Enabled" && valInVPD.at(0) != 2)
    {
        vpdVal.emplace_back(2);
    }
    else if (mirrorMode == "Disabled" && valInVPD.at(0) != 1)
    {
        vpdVal.emplace_back(1);
    }

    if (!vpdVal.empty())
    {
        std::cout << "Writing AMM to VPD: " << static_cast<int>(vpdVal.at(0))
                  << std::endl;
        manager.writeKeyword(sdbusplus::message::object_path{SYSTEM_OBJECT},
                             "UTIL", "D0", vpdVal);
    }
}

void BiosHandler::saveKeepAndClearToVPD(const std::string& keepAndClear)
{
    types::Binary vpdVal;
    auto valInVPD = readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.UTIL", "D1");

    if (valInVPD.size() != 1)
    {
        std::cerr << "Read bad size for UTIL/D1: " << valInVPD.size()
                  << std::endl;
        return;
    }

    if (keepAndClear != "Enabled" && keepAndClear != "Disabled")
    {
        std::cerr << "Bad value for keep and clear BIOS attribute: "
                  << keepAndClear << std::endl;
        return;
    }

    // Write to VPD only if the value is not already what we want to write.
    if (keepAndClear == "Enabled" && ((valInVPD.at(0) & 0x01) != 0x01))
    {
        vpdVal.emplace_back(valInVPD.at(0) | 0x01);
    }
    else if (keepAndClear == "Disabled" && ((valInVPD.at(0) & 0x01) != 0))
    {
        vpdVal.emplace_back(valInVPD.at(0) & ~(0x01));
    }

    if (!vpdVal.empty())
    {
        std::cout << "Writing Keep and Clear to VPD: "
                  << static_cast<int>(vpdVal.at(0)) << std::endl;
        manager.writeKeyword(sdbusplus::message::object_path{SYSTEM_OBJECT},
                             "UTIL", "D1", vpdVal);
    }
}

void BiosHandler::saveCreateDefaultLparToVPD(
    const std::string& createDefaultLpar)
{
    types::Binary vpdVal;
    auto valInVPD = readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.UTIL", "D1");

    if (valInVPD.size() != 1)
    {
        std::cerr << "Read bad size for UTIL/D1: " << valInVPD.size()
                  << std::endl;
        return;
    }

    if (createDefaultLpar != "Enabled" && createDefaultLpar != "Disabled")
    {
        std::cerr << "Bad value for create default lpar BIOS attribute: "
                  << createDefaultLpar << std::endl;
        return;
    }

    // Write to VPD only if the value is not already what we want to write.
    if (createDefaultLpar == "Enabled" && ((valInVPD.at(0) & 0x02) != 0x02))
    {
        vpdVal.emplace_back(valInVPD.at(0) | 0x02);
    }
    else if (createDefaultLpar == "Disabled" && ((valInVPD.at(0) & 0x02) != 0))
    {
        vpdVal.emplace_back(valInVPD.at(0) & ~(0x02));
    }

    if (!vpdVal.empty())
    {
        std::cout << "Writing create default lpar to VPD: "
                  << static_cast<int>(vpdVal.at(0)) << std::endl;
        manager.writeKeyword(sdbusplus::message::object_path{SYSTEM_OBJECT},
                             "UTIL", "D1", vpdVal);
    }
}

void BiosHandler::saveClearNVRAMToVPD(const std::string& clearNVRAM)
{
    types::Binary vpdVal;
    auto valInVPD = readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.UTIL", "D1");

    if (valInVPD.size() != 1)
    {
        std::cerr << "Read bad size for UTIL/D1: " << valInVPD.size()
                  << std::endl;
        return;
    }

    if (clearNVRAM != "Enabled" && clearNVRAM != "Disabled")
    {
        std::cerr << "Bad value for clear NVRAM BIOS attribute: " << clearNVRAM
                  << std::endl;
        return;
    }

    // Write to VPD only if the value is not already what we want to write.
    if (clearNVRAM == "Enabled" && ((valInVPD.at(0) & 0x04) != 0x04))
    {
        vpdVal.emplace_back(valInVPD.at(0) | 0x04);
    }
    else if (clearNVRAM == "Disabled" && ((valInVPD.at(0) & 0x04) != 0))
    {
        vpdVal.emplace_back(valInVPD.at(0) & ~(0x04));
    }

    if (!vpdVal.empty())
    {
        std::cout << "Writing clear NVRAM to VPD: "
                  << static_cast<int>(vpdVal.at(0)) << std::endl;
        manager.writeKeyword(sdbusplus::message::object_path{SYSTEM_OBJECT},
                             "UTIL", "D1", vpdVal);
    }
}

int64_t BiosHandler::readBIOSFCO()
{
    int64_t fcoVal = -1;
    auto val = readBIOSAttribute("hb_field_core_override");

    if (auto pVal = std::get_if<int64_t>(&val))
    {
        fcoVal = *pVal;
    }
    else
    {
        std::cerr << "FCO is not an int" << std::endl;
    }
    return fcoVal;
}

std::string BiosHandler::readBIOSAMM()
{
    std::string ammVal{};
    auto val = readBIOSAttribute("hb_memory_mirror_mode");

    if (auto pVal = std::get_if<std::string>(&val))
    {
        ammVal = *pVal;
    }
    else
    {
        std::cerr << "AMM is not a string" << std::endl;
    }
    return ammVal;
}

std::string BiosHandler::readBIOSKeepAndClear()
{
    std::string keepAndClear{};
    auto val = readBIOSAttribute("pvm_keep_and_clear");

    if (auto pVal = std::get_if<std::string>(&val))
    {
        keepAndClear = *pVal;
    }
    else
    {
        std::cerr << "Keep and clear is not a string" << std::endl;
    }
    return keepAndClear;
}

std::string BiosHandler::readBIOSCreateDefaultLpar()
{
    std::string createDefaultLpar{};
    auto val = readBIOSAttribute("pvm_create_default_lpar");

    if (auto pVal = std::get_if<std::string>(&val))
    {
        createDefaultLpar = *pVal;
    }
    else
    {
        std::cerr << "Create default LPAR is not a string" << std::endl;
    }
    return createDefaultLpar;
}

std::string BiosHandler::readBIOSClearNVRAM()
{
    std::string clearNVRAM{};
    auto val = readBIOSAttribute("pvm_clear_nvram");

    if (auto pVal = std::get_if<std::string>(&val))
    {
        clearNVRAM = *pVal;
    }
    else
    {
        std::cerr << "Clear NVRAM is not a string" << std::endl;
    }
    return clearNVRAM;
}

void BiosHandler::saveFCOToBIOS(const std::string& fcoVal, int64_t fcoInBIOS)
{
    if (fcoVal.size() != 4)
    {
        std::cerr << "Bad size for FCO in VPD: " << fcoVal.size() << std::endl;
        return;
    }

    // Need to write?
    if (fcoInBIOS == static_cast<int64_t>(fcoVal.at(3)))
    {
        std::cout << "Skip FCO BIOS write, value is already: " << fcoInBIOS
                  << std::endl;
        return;
    }

    types::PendingBIOSAttrsType biosAttrs;
    biosAttrs.push_back(
        std::make_pair("hb_field_core_override",
                       std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                       "AttributeType.Integer",
                                       fcoVal.at(3))));

    std::cout << "Set hb_field_core_override to: "
              << static_cast<int>(fcoVal.at(3)) << std::endl;

    setBusProperty<types::PendingBIOSAttrsType>(
        "xyz.openbmc_project.BIOSConfigManager",
        "/xyz/openbmc_project/bios_config/manager",
        "xyz.openbmc_project.BIOSConfig.Manager", "PendingAttributes",
        biosAttrs);
}

void BiosHandler::saveAMMToBIOS(const std::string& ammVal,
                                const std::string& ammInBIOS)
{
    if (ammVal.size() != 1)
    {
        std::cerr << "Bad size for AMM in VPD: " << ammVal.size() << std::endl;
        return;
    }

    // Make sure data in VPD is sane
    if (ammVal.at(0) != 1 && ammVal.at(0) != 2)
    {
        std::cerr << "Bad value for AMM read from VPD: "
                  << static_cast<int>(ammVal.at(0)) << std::endl;
        return;
    }

    // Need to write?
    std::string toWrite = (ammVal.at(0) == 2) ? "Enabled" : "Disabled";
    if (ammInBIOS == toWrite)
    {
        std::cout << "Skip AMM BIOS write, value is already: " << toWrite
                  << std::endl;
        return;
    }

    types::PendingBIOSAttrsType biosAttrs;
    biosAttrs.push_back(
        std::make_pair("hb_memory_mirror_mode",
                       std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                       "AttributeType.Enumeration",
                                       toWrite)));

    std::cout << "Set hb_memory_mirror_mode to: " << toWrite << std::endl;

    setBusProperty<types::PendingBIOSAttrsType>(
        "xyz.openbmc_project.BIOSConfigManager",
        "/xyz/openbmc_project/bios_config/manager",
        "xyz.openbmc_project.BIOSConfig.Manager", "PendingAttributes",
        biosAttrs);
}

void BiosHandler::saveKeepAndClearToBIOS(const std::string& keepAndClear,
                                         const std::string& keepAndClearInBIOS)
{
    if (keepAndClear.size() != 1)
    {
        std::cerr << "Bad size for Keep and Clear in VPD: "
                  << keepAndClear.size() << std::endl;
        return;
    }

    // Need to write?
    std::string toWrite = (keepAndClear.at(0) & 0x01) ? "Enabled" : "Disabled";
    if (keepAndClearInBIOS == toWrite)
    {
        std::cout << "Skip Keep and Clear BIOS write, value is already: "
                  << toWrite << std::endl;
        return;
    }

    types::PendingBIOSAttrsType biosAttrs;
    biosAttrs.push_back(
        std::make_pair("pvm_keep_and_clear",
                       std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                       "AttributeType.Enumeration",
                                       toWrite)));

    std::cout << "Set pvm_keep_and_clear to: " << toWrite << std::endl;

    setBusProperty<types::PendingBIOSAttrsType>(
        "xyz.openbmc_project.BIOSConfigManager",
        "/xyz/openbmc_project/bios_config/manager",
        "xyz.openbmc_project.BIOSConfig.Manager", "PendingAttributes",
        biosAttrs);
}

void BiosHandler::saveCreateDefaultLparToBIOS(
    const std::string& createDefaultLpar,
    const std::string& createDefaultLparInBIOS)
{
    if (createDefaultLpar.size() != 1)
    {
        std::cerr << "Bad size for Create default LPAR in VPD: "
                  << createDefaultLpar.size() << std::endl;
        return;
    }

    // Need to write?
    std::string toWrite =
        (createDefaultLpar.at(0) & 0x02) ? "Enabled" : "Disabled";
    if (createDefaultLparInBIOS == toWrite)
    {
        std::cout << "Skip Create default LPAR BIOS write, value is already: "
                  << toWrite << std::endl;
        return;
    }

    types::PendingBIOSAttrsType biosAttrs;
    biosAttrs.push_back(
        std::make_pair("pvm_create_default_lpar",
                       std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                       "AttributeType.Enumeration",
                                       toWrite)));

    std::cout << "Set pvm_create_default_lpar to: " << toWrite << std::endl;

    setBusProperty<types::PendingBIOSAttrsType>(
        "xyz.openbmc_project.BIOSConfigManager",
        "/xyz/openbmc_project/bios_config/manager",
        "xyz.openbmc_project.BIOSConfig.Manager", "PendingAttributes",
        biosAttrs);
}

void BiosHandler::saveClearNVRAMToBIOS(const std::string& clearNVRAM,
                                       const std::string& clearNVRAMInBIOS)
{
    if (clearNVRAM.size() != 1)
    {
        std::cerr << "Bad size for Clear NVRAM in VPD: " << clearNVRAM.size()
                  << std::endl;
        return;
    }

    // Need to write?
    std::string toWrite = (clearNVRAM.at(0) & 0x04) ? "Enabled" : "Disabled";
    if (clearNVRAMInBIOS == toWrite)
    {
        std::cout << "Skip Clear NVRAM BIOS write, value is already: "
                  << toWrite << std::endl;
        return;
    }

    types::PendingBIOSAttrsType biosAttrs;
    biosAttrs.push_back(
        std::make_pair("pvm_clear_nvram",
                       std::make_tuple("xyz.openbmc_project.BIOSConfig.Manager."
                                       "AttributeType.Enumeration",
                                       toWrite)));

    std::cout << "Set pvm_clear_nvram to: " << toWrite << std::endl;

    setBusProperty<types::PendingBIOSAttrsType>(
        "xyz.openbmc_project.BIOSConfigManager",
        "/xyz/openbmc_project/bios_config/manager",
        "xyz.openbmc_project.BIOSConfig.Manager", "PendingAttributes",
        biosAttrs);
}

void BiosHandler::restoreBIOSAttribs()
{
    // TODO: We could make this slightly more scalable by defining a table of
    // attributes and their corresponding VPD keywords. However, that needs much
    // more thought.
    std::cout << "Attempting BIOS attribute reset" << std::endl;
    // Check if the VPD contains valid data for FCO, AMM, Keep and Clear,
    // Create default LPAR and Clear NVRAM *and* that it differs from the data
    // already in the attributes. If so, set the BIOS attributes as per the
    // value in the VPD. If the VPD contains default data, then initialize the
    // VPD keywords with data taken from the BIOS.
    auto fcoInVPD = readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.VSYS", "RG");
    auto ammInVPD = readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.UTIL", "D0");
    auto keepAndClearInVPD =
        readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.UTIL", "D1");
    auto fcoInBIOS = readBIOSFCO();
    auto ammInBIOS = readBIOSAMM();
    auto keepAndClearInBIOS = readBIOSKeepAndClear();
    auto createDefaultLparInBIOS = readBIOSCreateDefaultLpar();
    auto clearNVRAMInBIOS = readBIOSClearNVRAM();

    if (fcoInVPD == "    ")
    {
        saveFCOToVPD(fcoInBIOS);
    }
    else
    {
        saveFCOToBIOS(fcoInVPD, fcoInBIOS);
    }

    if (ammInVPD.at(0) == 0)
    {
        saveAMMToVPD(ammInBIOS);
    }
    else
    {
        saveAMMToBIOS(ammInVPD, ammInBIOS);
    }

    // No uninitialized handling needed for keep and clear, create default
    // lpar and clear nvram attributes. Their defaults in VPD are 0's which is
    // what we want.
    saveKeepAndClearToBIOS(keepAndClearInVPD, keepAndClearInBIOS);
    // Have to read D1 again because two attributes are stored in the same
    // keyword.
    auto createDefaultLparInVPD =
        readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.UTIL", "D1");
    saveCreateDefaultLparToBIOS(createDefaultLparInVPD,
                                createDefaultLparInBIOS);

    auto clearNVRAMInVPD =
        readBusProperty(SYSTEM_OBJECT, "com.ibm.ipzvpd.UTIL", "D1");
    saveClearNVRAMToBIOS(clearNVRAMInVPD, clearNVRAMInBIOS);

    // Start listener now that we have done the restore
    listenBiosAttribs();
}
} // namespace manager
} // namespace vpd
} // namespace openpower