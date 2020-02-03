#include <algorithm>
#include <com/ibm/vpd/Manager/server.hpp>
#include <com/ibm/vpd/error.hpp>
#include <map>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/server.hpp>
#include <string>
#include <tuple>
#include <variant>
#include <xyz/openbmc_project/Common/error.hpp>

namespace sdbusplus
{
namespace com
{
namespace ibm
{
namespace vpd
{
namespace server
{

Manager::Manager(bus::bus& bus, const char* path) :
    _com_ibm_vpd_Manager_interface(bus, path, interface, _vtable, this),
    _intf(bus.getInterface())
{
}

int Manager::_callback_WriteKeyword(sd_bus_message* msg, void* context,
                                    sd_bus_error* error)
{
    try
    {
        auto m = message::message(msg);
#if 1
        {
            auto tbus = m.get_bus();
            sdbusplus::server::transaction::Transaction t(tbus, m);
            sdbusplus::server::transaction::set_id(
                std::hash<sdbusplus::server::transaction::Transaction>{}(t));
        }
#endif

        sdbusplus::message::object_path path{};
        std::string record{};
        std::string keyword{};
        std::vector<uint8_t> value{};

        m.read(path, record, keyword, value);

        auto o = static_cast<Manager*>(context);
        o->writeKeyword(path, record, keyword, value);

        auto reply = m.new_method_return();
        // No data to append on reply.

        reply.method_return();
    }
    catch (sdbusplus::internal_exception_t& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }

    return true;
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
    try
    {
        auto m = message::message(msg);
#if 1
        {
            auto tbus = m.get_bus();
            sdbusplus::server::transaction::Transaction t(tbus, m);
            sdbusplus::server::transaction::set_id(
                std::hash<sdbusplus::server::transaction::Transaction>{}(t));
        }
#endif

        std::string locationCode{};
        uint16_t nodeNumber{};

        m.read(locationCode, nodeNumber);

        auto o = static_cast<Manager*>(context);
        auto r = o->getFRUsByUnexpandedLocationCode(locationCode, nodeNumber);

        auto reply = m.new_method_return();
        reply.append(std::move(r));

        reply.method_return();
    }
    catch (sdbusplus::internal_exception_t& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }
    catch (sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }
    catch (sdbusplus::com::ibm::vpd::Error::NotFound& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }

    return true;
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
    try
    {
        auto m = message::message(msg);
#if 1
        {
            auto tbus = m.get_bus();
            sdbusplus::server::transaction::Transaction t(tbus, m);
            sdbusplus::server::transaction::set_id(
                std::hash<sdbusplus::server::transaction::Transaction>{}(t));
        }
#endif

        std::string locationCode{};

        m.read(locationCode);

        auto o = static_cast<Manager*>(context);
        auto r = o->getFRUsByExpandedLocationCode(locationCode);

        auto reply = m.new_method_return();
        reply.append(std::move(r));

        reply.method_return();
    }
    catch (sdbusplus::internal_exception_t& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }
    catch (sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }
    catch (sdbusplus::com::ibm::vpd::Error::NotFound& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }

    return true;
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
    try
    {
        auto m = message::message(msg);
#if 1
        {
            auto tbus = m.get_bus();
            sdbusplus::server::transaction::Transaction t(tbus, m);
            sdbusplus::server::transaction::set_id(
                std::hash<sdbusplus::server::transaction::Transaction>{}(t));
        }
#endif

        std::string locationCode{};
        uint16_t nodeNumber{};

        m.read(locationCode, nodeNumber);

        auto o = static_cast<Manager*>(context);
        auto r = o->getExpandedLocationCode(locationCode, nodeNumber);

        auto reply = m.new_method_return();
        reply.append(std::move(r));

        reply.method_return();
    }
    catch (sdbusplus::internal_exception_t& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }
    catch (sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument& e)
    {
        return sd_bus_error_set(error, e.name(), e.description());
    }

    return true;
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
    vtable::end()};

} // namespace server
} // namespace vpd
} // namespace ibm
} // namespace com
} // namespace sdbusplus
