#include <algorithm>
#include <com/ibm/VPD/Manager/server.hpp>
#include <com/ibm/VPD/error.hpp>
#include <map>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/sdbuspp_support/server.hpp>
#include <sdbusplus/server.hpp>
#include <string>
#include <tuple>
#include <xyz/openbmc_project/Common/error.hpp>

namespace sdbusplus
{
namespace com
{
namespace ibm
{
namespace VPD
{
namespace server
{

Manager::Manager(bus_t& bus, const char* path) :
    _com_ibm_VPD_Manager_interface(bus, path, interface, _vtable, this),
    _intf(bus.getInterface())
{
}

int Manager::_callback_WriteKeyword(sd_bus_message* msg, void* context,
                                    sd_bus_error* error)
{
    auto o = static_cast<Manager*>(context);

    try
    {
        return sdbusplus::sdbuspp::method_callback(
            msg, o->_intf, error,
            std::function([=](sdbusplus::message::object_path&& path,
                              std::string&& record, std::string&& keyword,
                              std::vector<uint8_t>&& value) {
                return o->writeKeyword(path, record, keyword, value);
            }));
    }
    catch (
        const sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument&
            e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::PathNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::RecordNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::KeywordNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }

    return 0;
}

namespace details
{
namespace Manager
{
static const auto _param_WriteKeyword = utility::tuple_to_array(
    message::types::type_id<sdbusplus::message::object_path, std::string,
                            std::string, std::vector<uint8_t>>());
static const auto _return_WriteKeyword =
    utility::tuple_to_array(std::make_tuple('\0'));
} // namespace Manager
} // namespace details

int Manager::_callback_GetFRUsByUnexpandedLocationCode(sd_bus_message* msg,
                                                       void* context,
                                                       sd_bus_error* error)
{
    auto o = static_cast<Manager*>(context);

    try
    {
        return sdbusplus::sdbuspp::method_callback(
            msg, o->_intf, error,
            std::function(
                [=](std::string&& locationCode, uint16_t&& nodeNumber) {
                    return o->getFRUsByUnexpandedLocationCode(locationCode,
                                                              nodeNumber);
                }));
    }
    catch (
        const sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument&
            e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::LocationNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::NodeNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }

    return 0;
}

namespace details
{
namespace Manager
{
static const auto _param_GetFRUsByUnexpandedLocationCode =
    utility::tuple_to_array(message::types::type_id<std::string, uint16_t>());
static const auto _return_GetFRUsByUnexpandedLocationCode =
    utility::tuple_to_array(message::types::type_id<
                            std::vector<sdbusplus::message::object_path>>());
} // namespace Manager
} // namespace details

int Manager::_callback_GetFRUsByExpandedLocationCode(sd_bus_message* msg,
                                                     void* context,
                                                     sd_bus_error* error)
{
    auto o = static_cast<Manager*>(context);

    try
    {
        return sdbusplus::sdbuspp::method_callback(
            msg, o->_intf, error,
            std::function([=](std::string&& locationCode) {
                return o->getFRUsByExpandedLocationCode(locationCode);
            }));
    }
    catch (
        const sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument&
            e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::LocationNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::NodeNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }

    return 0;
}

namespace details
{
namespace Manager
{
static const auto _param_GetFRUsByExpandedLocationCode =
    utility::tuple_to_array(message::types::type_id<std::string>());
static const auto _return_GetFRUsByExpandedLocationCode =
    utility::tuple_to_array(message::types::type_id<
                            std::vector<sdbusplus::message::object_path>>());
} // namespace Manager
} // namespace details

int Manager::_callback_GetExpandedLocationCode(sd_bus_message* msg,
                                               void* context,
                                               sd_bus_error* error)
{
    auto o = static_cast<Manager*>(context);

    try
    {
        return sdbusplus::sdbuspp::method_callback(
            msg, o->_intf, error,
            std::function(
                [=](std::string&& locationCode, uint16_t&& nodeNumber) {
                    return o->getExpandedLocationCode(locationCode, nodeNumber);
                }));
    }
    catch (
        const sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument&
            e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::LocationNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }
    catch (const sdbusplus::com::ibm::VPD::Error::NodeNotFound& e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }

    return 0;
}

namespace details
{
namespace Manager
{
static const auto _param_GetExpandedLocationCode =
    utility::tuple_to_array(message::types::type_id<std::string, uint16_t>());
static const auto _return_GetExpandedLocationCode =
    utility::tuple_to_array(message::types::type_id<std::string>());
} // namespace Manager
} // namespace details

int Manager::_callback_PerformVPDRecollection(sd_bus_message* msg,
                                              void* context,
                                              sd_bus_error* error)
{
    auto o = static_cast<Manager*>(context);

    try
    {
        return sdbusplus::sdbuspp::method_callback(
            msg, o->_intf, error,
            std::function([=]() { return o->performVPDRecollection(); }));
    }
    catch (
        const sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument&
            e)
    {
        return o->_intf->sd_bus_error_set(error, e.name(), e.description());
    }

    return 0;
}

namespace details
{
namespace Manager
{
static const auto _param_PerformVPDRecollection =
    utility::tuple_to_array(std::make_tuple('\0'));
static const auto _return_PerformVPDRecollection =
    utility::tuple_to_array(std::make_tuple('\0'));
} // namespace Manager
} // namespace details

const vtable::vtable_t Manager::_vtable[] = {
    vtable::start(),

    vtable::method("WriteKeyword", details::Manager::_param_WriteKeyword.data(),
                   details::Manager::_return_WriteKeyword.data(),
                   _callback_WriteKeyword),

    vtable::method(
        "GetFRUsByUnexpandedLocationCode",
        details::Manager::_param_GetFRUsByUnexpandedLocationCode.data(),
        details::Manager::_return_GetFRUsByUnexpandedLocationCode.data(),
        _callback_GetFRUsByUnexpandedLocationCode),

    vtable::method(
        "GetFRUsByExpandedLocationCode",
        details::Manager::_param_GetFRUsByExpandedLocationCode.data(),
        details::Manager::_return_GetFRUsByExpandedLocationCode.data(),
        _callback_GetFRUsByExpandedLocationCode),

    vtable::method("GetExpandedLocationCode",
                   details::Manager::_param_GetExpandedLocationCode.data(),
                   details::Manager::_return_GetExpandedLocationCode.data(),
                   _callback_GetExpandedLocationCode),

    vtable::method("PerformVPDRecollection",
                   details::Manager::_param_PerformVPDRecollection.data(),
                   details::Manager::_return_PerformVPDRecollection.data(),
                   _callback_PerformVPDRecollection),
    vtable::end()};

} // namespace server
} // namespace VPD
} // namespace ibm
} // namespace com
} // namespace sdbusplus
