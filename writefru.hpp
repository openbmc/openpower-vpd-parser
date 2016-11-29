// WARNING: Generated header. Do not edit!


#pragma once

#include <map>
#include <iostream>
#include "defines.hpp"
#include "store.hpp"

namespace openpower
{
namespace vpd
{
namespace inventory
{

using Inner = Parsed::mapped_type;
using Outer = std::map<std::string, Inner>;

// TODO: Remove once the call to inventory manager is added
auto print = [](Outer&& object, const std::string& path)
{
    std::cout << "\n";
    std::cout << path << "\n";
    std::cout << "\n";
    for(const auto& o : object)
    {
        std::cout << o.first << "\n";
        for(const auto& i : o.second)
        {
            std::cout << i.first << " : " << i.second << "\n";
        }
        std::cout << "\n";
    }
};

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
    Outer object;

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    Inner xyzAsset;
    xyzAsset["PartNumber"] =
        vpdStore.get<Record::VINI, record::Keyword::PN>();
    xyzAsset["SerialNumber"] =
        vpdStore.get<Record::VINI, record::Keyword::SN>();
    xyzAsset["Manufacturer"] =
        vpdStore.get<Record::OPFR, record::Keyword::VN>();
    object.emplace("xyz.openbmc_project.Inventory.Decorator.Asset",
                   std::move(xyzAsset));
    Inner xyzRevision;
    xyzRevision["Version"] =
        vpdStore.get<Record::VINI, record::Keyword::HW>();
    object.emplace("xyz.openbmc_project.Inventory.Decorator.Revision",
                   std::move(xyzRevision));
    Inner xyzItem;
    xyzItem["PrettyName"] =
        vpdStore.get<Record::VINI, record::Keyword::DR>();
    object.emplace("xyz.openbmc_project.Inventory.Item",
                   std::move(xyzItem));
    Inner orgAsset;
    orgAsset["CCIN"] =
        vpdStore.get<Record::VINI, record::Keyword::CC>();
    object.emplace("org.openpower_project.Inventory.Decorator.Asset",
                   std::move(orgAsset));

    // TODO: Need integration with inventory manager, print serialized dbus
    // object for now.
    print(std::move(object), path);
}

// Specialization of ETHERNET
template<>
void writeFru<Fru::ETHERNET>(const Store& vpdStore,
                           const std::string& path)
{
    Outer object;

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    Inner xyzNetworkInterface;
    xyzNetworkInterface["MACAddress"] =
        vpdStore.get<Record::VINI, record::Keyword::B1>();
    object.emplace("xyz.openbmc_project.Inventory.Item.NetworkInterface",
                   std::move(xyzNetworkInterface));

    // TODO: Need integration with inventory manager, print serialized dbus
    // object for now.
    print(std::move(object), path);
}

} // namespace inventory
} // namespace vpd
} // namespace openpower
