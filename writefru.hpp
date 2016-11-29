/* !!! WARNING: Generated header. Do not edit. !!! */

#pragma once

#include <map>
#include <iostream>
#include <defines.hpp>
#include <store.hpp>

namespace openpower
{
namespace vpd
{
namespace inventory
{

using inner = std::map<std::string, std::string>;
using outer = std::map<std::string, inner>;

auto print = [](outer&& object,const std::string& path)
{
    std::cout << std::endl;
    std::cout << path << std::endl;
    std::cout << std::endl;
    for(const auto& o : object)
    {
        std::cout << o.first << std::endl;
        for(const auto& i : o.second)
        {
            std::cout << i.first << " : " << i.second << std::endl;
        }
        std::cout << std::endl;
    }
};

/** @brief API to write parsed VPD to inventory, for a specifc FRU
 *
 *  @param [in] vpdStore - Store object containing parsed VPD
 *  @param [in] path - FRU object path
 */
template<Fru F>
void writeFru(Store&& vpdStore,
    const std::string& path);

/** @brief Specialization of BMC */
template<>
void writeFru<Fru::BMC>(Store&& vpdStore,
    const std::string& path)
{
    outer object;

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces.
    inner xyzAsset;
    xyzAsset["PartNumber"] =
    vpdStore.get<Record::VINI, record::Keyword::PN>();
    xyzAsset["SerialNumber"] =
    vpdStore.get<Record::VINI, record::Keyword::SN>();
    xyzAsset["Manufacturer"] =
    vpdStore.get<Record::OPFR, record::Keyword::VN>();
    object.emplace("xyz.openbmc_project.Inventory.Decorator.Asset",
        xyzAsset);

    inner xyzRevision;
    xyzRevision["Version"] =
    vpdStore.get<Record::VINI, record::Keyword::HW>();
    object.emplace("xyz.openbmc_project.Inventory.Decorator.Revision",
        xyzRevision);

    inner xyzItem;
    xyzItem["PrettyName"] =
    vpdStore.get<Record::VINI, record::Keyword::DR>();
    object.emplace("xyz.openbmc_project.Inventory.Item",
        xyzItem);

    inner orgAsset;
    orgAsset["CCIN"] =
    vpdStore.get<Record::VINI, record::Keyword::CC>();
    object.emplace("org.openpower_project.Inventory.Decorator.Asset",
        orgAsset);

    // TODO: Need integration with inventory manager, print serialized dbus
    // object for now.
    print(std::move(object), path);
}

/** @brief Specialization of ETHERNET */
template<>
void writeFru<Fru::ETHERNET>(Store&& vpdStore,
    const std::string& path)
{
    outer object;

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces.
    inner xyzNetworkInterface;
    xyzNetworkInterface["MACAddress"] =
    vpdStore.get<Record::VINI, record::Keyword::B1>();
    object.emplace("xyz.openbmc_project.Inventory.Item.NetworkInterface",
        xyzNetworkInterface);

    // TODO: Need integration with inventory manager, print serialized dbus
    // object for now.
    print(std::move(object), path);
}

} // namespace inventory
} // namespace vpd
} // namespace openpower
