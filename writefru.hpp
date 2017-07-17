// WARNING: Generated header. Do not edit!


#pragma once

#include <map>
#include <iostream>
#include "defines.hpp"
#include "store.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "extra-properties-gen.hpp"

namespace openpower
{
namespace vpd
{
namespace inventory
{

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
    auto iter = extra::objects.find(path);

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    PropertyMap xyzAssetProps;
    xyzAssetProps["PartNumber"] =
        vpdStore.get<Record::VINI, record::Keyword::PN>();
    xyzAssetProps["SerialNumber"] =
        vpdStore.get<Record::VINI, record::Keyword::SN>();
    xyzAssetProps["Manufacturer"] =
        vpdStore.get<Record::OPFR, record::Keyword::VN>();
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        auto propIter = (iter->second).find("xyz.openbmc_project.Inventory.Decorator.Asset");
        if((iter->second).end() != propIter)
        {
            for(const auto& map : propIter->second)
            {
                xyzAssetProps[map.first] = map.second;
            }
        }
    }
    interfaces.emplace("xyz.openbmc_project.Inventory.Decorator.Asset",
                       std::move(xyzAssetProps));
    PropertyMap xyzUUIDProps;
    xyzUUIDProps["uuid"] =
        vpdStore.get<Record::OPFR, record::Keyword::MM>();
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        auto propIter = (iter->second).find("xyz.openbmc_project.Common.UUID");
        if((iter->second).end() != propIter)
        {
            for(const auto& map : propIter->second)
            {
                xyzUUIDProps[map.first] = map.second;
            }
        }
    }
    interfaces.emplace("xyz.openbmc_project.Common.UUID",
                       std::move(xyzUUIDProps));
    PropertyMap xyzRevisionProps;
    xyzRevisionProps["Version"] =
        vpdStore.get<Record::VINI, record::Keyword::HW>();
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        auto propIter = (iter->second).find("xyz.openbmc_project.Inventory.Decorator.Revision");
        if((iter->second).end() != propIter)
        {
            for(const auto& map : propIter->second)
            {
                xyzRevisionProps[map.first] = map.second;
            }
        }
    }
    interfaces.emplace("xyz.openbmc_project.Inventory.Decorator.Revision",
                       std::move(xyzRevisionProps));
    PropertyMap xyzItemProps;
    xyzItemProps["PrettyName"] =
        vpdStore.get<Record::VINI, record::Keyword::DR>();
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        auto propIter = (iter->second).find("xyz.openbmc_project.Inventory.Item");
        if((iter->second).end() != propIter)
        {
            for(const auto& map : propIter->second)
            {
                xyzItemProps[map.first] = map.second;
            }
        }
    }
    interfaces.emplace("xyz.openbmc_project.Inventory.Item",
                       std::move(xyzItemProps));
    PropertyMap orgAssetProps;
    orgAssetProps["CCIN"] =
        vpdStore.get<Record::VINI, record::Keyword::CC>();
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        auto propIter = (iter->second).find("org.openpower_project.Inventory.Decorator.Asset");
        if((iter->second).end() != propIter)
        {
            for(const auto& map : propIter->second)
            {
                orgAssetProps[map.first] = map.second;
            }
        }
    }
    interfaces.emplace("org.openpower_project.Inventory.Decorator.Asset",
                       std::move(orgAssetProps));

    sdbusplus::message::object_path object(path);
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        for(const auto& entry : iter->second)
        {
            if(interfaces.end() == interfaces.find(entry.first))
            {
                interfaces.emplace(entry.first, entry.second);
            }
        }
    }
    objects.emplace(std::move(object), std::move(interfaces));

    callPIM(std::move(objects));
}

// Specialization of ETHERNET
template<>
void writeFru<Fru::ETHERNET>(const Store& vpdStore,
                           const std::string& path)
{
    ObjectMap objects;
    InterfaceMap interfaces;
    auto iter = extra::objects.find(path);

    // Inventory manager needs object path, list of interface names to be
    // implemented, and property:value pairs contained in said interfaces

    PropertyMap xyzNetworkInterfaceProps;
    xyzNetworkInterfaceProps["MACAddress"] =
        vpdStore.get<Record::VINI, record::Keyword::B1>();
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        auto propIter = (iter->second).find("xyz.openbmc_project.Inventory.Item.NetworkInterface");
        if((iter->second).end() != propIter)
        {
            for(const auto& map : propIter->second)
            {
                xyzNetworkInterfaceProps[map.first] = map.second;
            }
        }
    }
    interfaces.emplace("xyz.openbmc_project.Inventory.Item.NetworkInterface",
                       std::move(xyzNetworkInterfaceProps));

    sdbusplus::message::object_path object(path);
    // Check and update extra properties
    if(extra::objects.end() != iter)
    {
        for(const auto& entry : iter->second)
        {
            if(interfaces.end() == interfaces.find(entry.first))
            {
                interfaces.emplace(entry.first, entry.second);
            }
        }
    }
    objects.emplace(std::move(object), std::move(interfaces));

    callPIM(std::move(objects));
}

} // namespace inventory
} // namespace vpd
} // namespace openpower
