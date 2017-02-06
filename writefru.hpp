// WARNING: Generated header. Do not edit!


#pragma once

#include <map>
#include <iostream>
#include <sdbusplus/server.hpp>
#include "defines.hpp"
#include "store.hpp"

namespace openpower
{
namespace vpd
{
namespace inventory
{

using Property = std::string;
using Value = sdbusplus::message::variant<bool, int64_t, std::string>;
using PropertyMap = std::map<Property, Value>;

using Interface = std::string;
using InterfaceMap = std::map<Interface, PropertyMap>;

using Object = sdbusplus::message::object_path;
using ObjectMap = std::map<Object, InterfaceMap>;

static constexpr auto pimPath = "/xyz/openbmc_project/Inventory";
static constexpr auto pimIntf = "xyz.openbmc_project.Inventory.Manager";

/** @brief Get inventory-manager's d-bus service
 */
auto getPIMService()
{
    auto bus = sdbusplus::bus::new_default();
    auto mapper =
        bus.new_method_call(
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/ObjectMapper",
            "xyz.openbmc_project.ObjectMapper",
            "GetObject");

    mapper.append(std::string(pimPath));
    mapper.append(std::vector<std::string>({std::string(pimIntf)}));

    auto result = bus.call(mapper);
    if(result.is_method_error())
    {
        throw std::runtime_error("ObjectMapper GetObject failed");
    }

    std::map<std::string, std::vector<std::string>> response;
    result.read(response);
    if(response.empty())
    {
        throw std::runtime_error("ObjectMapper GetObject bad response");
    }

    return response.begin()->first;
}

auto callPIM(ObjectMap&& objects)
{
    auto notify = [&](const char* pimService)
    {
        auto bus = sdbusplus::bus::new_default();
        return
            bus.new_method_call(
                pimService,
                pimPath,
                pimIntf,
                "Notify");
    };

    std::string service;
    try
    {
        service = getPIMService();
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << e.what() << "\n";
        return;
    }
    auto bus = sdbusplus::bus::new_default();
    auto pim = notify(service.c_str());
    pim.append(objects);
    auto result = bus.call(pim);
    if(result.is_method_error())
    {
        std::cerr << "PIM Notify() failed\n";
    }
}

/** @brief API to write parsed VPD to inventory,
 *      for a specifc FRU
 *
 *  @param [in] vpdStore - Store object containing
 *      parsed VPD
 *  @param [in] path - FRU object path
 */
template<Fru F>
void writeFru(const Store& vpdStore, const std::string& path);

// Specialization of BMC
template<>
void writeFru<Fru::BMC>(const Store& vpdStore,
                           const std::string& path)
{
    ObjectMap objects;
    InterfaceMap interfaces;

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    PropertyMap xyzAssetProps;
    xyzAssetProps["PartNumber"] =
        vpdStore.get<Record::VINI, record::Keyword::PN>();
    xyzAssetProps["SerialNumber"] =
        vpdStore.get<Record::VINI, record::Keyword::SN>();
    xyzAssetProps["Manufacturer"] =
        vpdStore.get<Record::OPFR, record::Keyword::VN>();
    interfaces.emplace("xyz.openbmc_project.Inventory.Decorator.Asset",
                       std::move(xyzAssetProps));
    PropertyMap xyzRevisionProps;
    xyzRevisionProps["Version"] =
        vpdStore.get<Record::VINI, record::Keyword::HW>();
    interfaces.emplace("xyz.openbmc_project.Inventory.Decorator.Revision",
                       std::move(xyzRevisionProps));
    PropertyMap xyzItemProps;
    xyzItemProps["PrettyName"] =
        vpdStore.get<Record::VINI, record::Keyword::DR>();
    interfaces.emplace("xyz.openbmc_project.Inventory.Item",
                       std::move(xyzItemProps));
    PropertyMap orgAssetProps;
    orgAssetProps["CCIN"] =
        vpdStore.get<Record::VINI, record::Keyword::CC>();
    interfaces.emplace("org.openpower_project.Inventory.Decorator.Asset",
                       std::move(orgAssetProps));

    sdbusplus::message::object_path object(path);
    objects.emplace(object, interfaces);

    callPIM(std::move(objects));
}

// Specialization of ETHERNET
template<>
void writeFru<Fru::ETHERNET>(const Store& vpdStore,
                           const std::string& path)
{
    ObjectMap objects;
    InterfaceMap interfaces;

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    PropertyMap xyzNetworkInterfaceProps;
    xyzNetworkInterfaceProps["MACAddress"] =
        vpdStore.get<Record::VINI, record::Keyword::B1>();
    interfaces.emplace("xyz.openbmc_project.Inventory.Item.NetworkInterface",
                       std::move(xyzNetworkInterfaceProps));

    sdbusplus::message::object_path object(path);
    objects.emplace(object, interfaces);

    callPIM(std::move(objects));
}

} // namespace inventory
} // namespace vpd
} // namespace openpower
