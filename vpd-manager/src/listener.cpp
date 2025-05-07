#include "config.h"

#include "listener.hpp"

#include "utility/json_utility.hpp"

#include <filesystem>
#include <iostream>

namespace vpd
{
void Listener::handleCorrelatedDbusProps(sdbusplus::message_t& i_msg)
{
    if (i_msg.is_method_error())
    {
        std::cerr << "\n Error in received callback: " << i_msg.get_error()
                  << ". Errno : " << i_msg.get_errno() << std::endl;
        return;
    }
    try
    {
        // get object path
        const auto l_objPath = i_msg.get_path();

        // get interface and properties
        std::string l_interface;
        std::unordered_map<std::string, std::variant<types::BinaryVector, bool,
                                                     std::string, uint8_t>>
            l_propMap;
        i_msg.read(l_interface, l_propMap);

        if (!m_parsedCorrelatedJson[constants::pimIntf].contains(l_interface))
        {
            std::cout << l_interface
                      << " not found in correlated_properties.json"
                      << std::endl;
            return;
        }

        for (auto l_propVal : l_propMap)
        {
            const auto l_property = l_propVal.first;

            if (m_parsedCorrelatedJson[constants::pimIntf][l_interface]
                    .contains(l_property))
            {
                const auto l_pathsPairDefined =
                    m_parsedCorrelatedJson[constants::pimIntf][l_interface]
                                          [l_property]
                                              .contains("pathsPair");
                bool l_inputPresentInPathsPair = false;

                if (l_pathsPairDefined)
                {
                    l_inputPresentInPathsPair =
                        m_parsedCorrelatedJson[constants::pimIntf][l_interface]
                                              [l_property]["pathsPair"]
                                                  .contains(l_objPath);
                }

                if (!l_pathsPairDefined || !l_inputPresentInPathsPair)
                {
                    // if default interface is present
                    if (m_parsedCorrelatedJson[constants::pimIntf][l_interface]
                                              [l_property]
                                                  .contains(
                                                      "defaultInterfaces"))
                    {
                        const auto l_fruPath = jsonUtility::getFruPathFromJson(
                            m_parsedInvJSON, l_objPath);

                        if (l_fruPath.empty())
                        {
                            std::cerr << "\nGiven inventory path " << l_objPath
                                      << " is not supported by vpd-manager."
                                      << std::endl;
                            return;
                        }

                        const nlohmann::json& l_inputFruObject =
                            m_parsedInvJSON["frus"][l_fruPath]
                                .get_ref<const nlohmann::json::array_t&>();

                        const bool l_inheritValue = true;

                        if (m_parsedInvJSON["frus"][l_fruPath][l_objPath]
                                .contains("inherit"))
                        {
                            l_inheritValue =
                                m_parsedInvJSON["frus"][l_fruPath][l_objPath]
                                               ["inherit"];
                        }

                        const nlohmann::json& l_defaultIntfObject =
                            m_parsedCorrelatedJson
                                [constants::pimIntf][l_interface][l_property]
                                ["defaultInterfaces"]
                                    .get_ref<const nlohmann::json::object_t&>();

                        const auto l_sourceType = "s";

                        if (m_parsedCorrelatedJson[constants::pimIntf]
                                                  [l_interface]
                                                      .contains(
                                                          "DbusSignature"))
                        {
                            l_sourceType =
                                m_parsedCorrelatedJson[constants::pimIntf]
                                                      [l_interface]
                                                      ["DbusSignature"];
                        }

                        if (!l_inheritValue)
                        {
                            types::ObjectMap l_dataToBePublished;

                            // update only the object path with default intf
                            for (const auto intfPropPair :
                                 l_defaultIntfObject.items())
                            {
                                const auto l_destinationInterface =
                                    intfPropPair.key();

                                // get the type that has to be updated
                                if (m_parsedCorrelatedJson
                                        [constants::pimIntf]
                                        [l_destinationInterface]
                                            .contains("DbusSignature"))
                                {
                                    l_destinationType = m_parsedCorrelatedJson
                                        [constants::pimIntf]
                                        [l_destinationInterface]
                                        ["DbusSignature"];
                                }

                                types::DbusVariantType l_finalData;

                                if (l_sourceType != l_destinationType)
                                {
                                    // convert the value to destination type
                                    l_finalData = convertToGivenType(
                                        types::DbusVariantType,
                                        l_destinationType);
                                }
                                else
                                {
                                    l_finalData = l_propVal.second;
                                }

                                l_dataToBePublished.emplace(
                                    l_objPath,
                                    types::InterfaceMap{std::make_pair(
                                        intfPropPair.key(),
                                        types::PropertyMap{
                                            std::make_pair(intfPropPair.value(),
                                                           l_finalData)})});
                            }

                            dbusUtility::callPIM(
                                std::move(l_dataToBePublished));
                        }
                        else
                        {
                            // if inherit is true, then update all paths in
                            // the group whose inherit is not false
                            for (const auto l_subFru : l_inputFruObject)
                            {}
                        }
                    }
                }
            }
        }

        std::cout << "\n l_objPath from get_path = " << l_objPath << std::endl;
        std::cout << "\ninterface read from i_msg" << l_interface << std::endl;
    }
    catch (const std::exception& l_ex)
    {
        std::cerr << "Error in handling callback - e.what = " << l_ex.what()
                  << std::endl;
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
                [this](sdbusplus::message_t& i_msg) {
                    handleCorrelatedDbusProps(i_msg);
                });

        // save the registered match objects
        m_listOfMatchObjects[constants::pimIntf][l_interface] = l_matchObj;
    }
}
} // namespace vpd
