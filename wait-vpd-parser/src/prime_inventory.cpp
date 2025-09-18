#include "prime_inventory.hpp"

#include "utility/dbus_utility.hpp"
#include "utility/json_utility.hpp"
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
        throw std::runtime_error("Mandatory tag(s) missing from JSON file [" +
                                 i_sysConfJsonPath + "]");
    }

    m_logger = vpd::Logger::getLoggerInstance();
}

bool PrimeInventory::isPrimingRequired() const noexcept
{
    // ToDo: Check if priming is required.
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
    [[maybe_unused]] const nlohmann::json& i_interfaceJson,
    [[maybe_unused]] vpd::types::InterfaceMap& io_interfaceMap,
    [[maybe_unused]] const vpd::types::VPDMapVariant& i_parsedVpdMap)
    const noexcept
{
    // ToDo: Populate interfaces needs to be published on Dbus.
}

void PrimeInventory::processFunctionalProperty(
    [[maybe_unused]] const std::string& i_inventoryObjPath,
    [[maybe_unused]] vpd::types::InterfaceMap& io_interfaces) const noexcept
{
    // ToDo: Populate interface to publish Functional property on Dbus.
}

void PrimeInventory::processEnabledProperty(
    [[maybe_unused]] const std::string& i_inventoryObjPath,
    [[maybe_unused]] vpd::types::InterfaceMap& io_interfaces) const noexcept
{
    // ToDo: Populate interface to publish Enabled property on Dbus.
}
