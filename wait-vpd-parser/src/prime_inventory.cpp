#include "prime_inventory.hpp"

#include "logger.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <string>

#define SYSTEM_VPD_FILE_PATH "/sys/bus/i2c/drivers/at24/8-0050/eeprom"

PrimeInventory::PrimeInventory(const std::string& i_sysConfJsonPath)
{
    uint16_t l_errCode = 0;
    m_sysCfgJsonObj =
        vpd::jsonUtility::getParsedJson(i_sysConfJsonPath, l_errCode);

    if (l_errCode)
    {
        throw std::runtime_error(
            "JSON parsing failed for file [ " + i_sysConfJsonPath +
            " ], error : " + vpd::vpdSpecificUtility::getErrCodeMsg(l_errCode));
    }

    // check for mandatory fields at this point itself.
    if (!m_sysCfgJsonObj.contains("frus"))
    {
        throw std::runtime_error("Mandatory tag(s) missing from JSON");
    }

    m_logger = vpd::Logger::getLoggerInstance();
}

bool PrimeInventory::isPrimingRequired() const noexcept
{
    // ToDo: check if priming is required
    return true;
}

void PrimeInventory::primeSystemBlueprint() const noexcept
{
    /*ToDo:
    Traverse the system config JSON &
    prime all the FRU paths which qualifies for priming.
    */
}

bool PrimeInventory::primeInventory(
    [[maybe_unused]] const std::string& i_vpdFilePath) const noexcept
{
    // ToDo: Travers system config JSON & prime inventory objects found under
    // the EEPROM.
    return true;
}

void PrimeInventory::populateInterfaces(
    [[maybe_unused]] const nlohmann::json& interfaceJson,
    [[maybe_unused]] vpd::types::InterfaceMap& interfaceMap,
    [[maybe_unused]] const vpd::types::VPDMapVariant& parsedVpdMap)
    const noexcept
{
    // ToDo: Populate interfaces needs to be published under the Dbus.
}

void PrimeInventory::processFunctionalProperty(
    [[maybe_unused]] const std::string& i_inventoryObjPath,
    [[maybe_unused]] vpd::types::InterfaceMap& io_interfaces) const noexcept
{
    // ToDo: Populate interface to publish Functional property under the Dbus.
}

void PrimeInventory::processEnabledProperty(
    [[maybe_unused]] const std::string& i_inventoryObjPath,
    [[maybe_unused]] vpd::types::InterfaceMap& io_interfaces) const noexcept
{
    // ToDo: Populate interface to publish Enabled property under the Dbus.
}
