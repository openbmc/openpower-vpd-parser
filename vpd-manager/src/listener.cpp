#include "config.h"

#include "listener.hpp"

#include "utility/json_utility.hpp"

#include <filesystem>
#include <iostream>

namespace vpd
{
static void handleCorrelatedDbusProps(sdbusplus::message_t& i_msg)
{
    if (i_msg.is_method_error())
    {
        std::cerr << "\n Error in received callback: " << i_msg.get_error()
                  << ". Errno : " << i_msg.get_errno() << std::endl;
    }
}

void Listener::regAllCorrelatedDbusProps()
{
    if (!std::filesystem::exists(CORRELATED_PROPS_JSON))
    {
        std::cerr << "\ncorrelated properties json is missing" << std::endl;
	return;
    }

    // parse correlated properties json.
    m_parsedCorrelatedJson = jsonUtility::getParsedJson(CORRELATED_PROPS_JSON);

    if (m_parsedCorrelatedJson.empty())
    {
        std::cerr << "\nFailed to parse correlated properties json."
                  << std::endl;
	return;
    }

    if (!m_parsedCorrelatedJson.contains(constants::pimIntf))
    {
        std::cerr
            << "\nMissing xyz.openbmc_project.Inventory.Manager section in correlated properties json."
            << std::endl;
	return;
    }

    const nlohmann::json& l_invServiceObj =
        m_parsedCorrelatedJson[constants::pimIntf]
            .get_ref<const nlohmann::json::object_t&>();

    // get all interfaces and register d-bus signal
    for (const auto& l_interfaceObj : l_invServiceObj.items())
    {
        const auto& l_interface = l_interfaceObj.key();

        // register d-bus properties changed signal for l_interface
        static std::shared_ptr<sdbusplus::bus::match::match> l_matchObj =
            std::make_unique<sdbusplus::bus::match::match>(
                static_cast<sdbusplus::bus_t&>(*m_asioConnection),
                "type='signal',member='PropertiesChanged',"
                "interface='org.freedesktop.DBus.Properties',"
                "sender='xyz.openbmc_project.Inventory.Manager',arg0='" +
                    l_interface + "'",
                handleCorrelatedDbusProps);

        // save the registered match objects
        m_listOfMatchObjects[constants::pimIntf][l_interface] = l_matchObj;
    }
}
} // namespace vpd
