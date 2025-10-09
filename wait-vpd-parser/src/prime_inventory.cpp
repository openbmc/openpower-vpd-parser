#include "prime_inventory.hpp"

#include "event_logger.hpp"
#include "exceptions.hpp"
#include "utility/common_utility.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/json_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <string>

PrimeInventory::PrimeInventory()
{
    try
    {
        uint16_t l_errCode = 0;
        m_sysCfgJsonObj =
            vpd::jsonUtility::getParsedJson(INVENTORY_JSON_SYM_LINK, l_errCode);

        if (l_errCode)
        {
            throw std::runtime_error(
                "JSON parsing failed for file [ " +
                std::string(INVENTORY_JSON_SYM_LINK) +
                " ], error : " + vpd::commonUtility::getErrCodeMsg(l_errCode));
        }

        // check for mandatory fields at this point itself.
        if (!m_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error(
                "Mandatory tag(s) missing from JSON file [" +
                std::string(INVENTORY_JSON_SYM_LINK) + "]");
        }
    }
    catch (const std::exception& l_ex)
    {
        vpd::EventLogger::createSyncPel(
            vpd::types::ErrorType::JsonFailure,
            vpd::types::SeverityType::Critical, __FILE__, __FUNCTION__, 0,
            "Prime inventory failed, reason: " + std::string(l_ex.what()),
            std::nullopt, std::nullopt, std::nullopt, std::nullopt);

        throw;
    }
}

bool PrimeInventory::isPrimingRequired() const noexcept
{
    // ToDo: Check if priming is required.
    return true;
}

void PrimeInventory::primeSystemBlueprint() const noexcept
{
    try
    {
        /*ToDo:
      * Check if priming is required.
      * Traverse the system config JSON &
      prime all the FRU paths which qualifies for priming.
      */
    }
    catch (const std::exception& l_ex)
    {
        // ToDo: log an error
    }
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
