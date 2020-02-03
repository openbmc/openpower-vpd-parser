#include <algorithm>
#include <map>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/server.hpp>
#include <string>
#include <tuple>
#include <variant>
#include <xyz/openbmc_project/Inventory/VPD/VPDKeywordEditor/server.hpp>

namespace sdbusplus
{
namespace xyz
{
namespace openbmc_project
{
namespace Inventory
{
namespace VPD
{
namespace server
{

VPDKeywordEditor::VPDKeywordEditor(bus::bus& bus, const char* path) :
    _xyz_openbmc_project_Inventory_VPD_VPDKeywordEditor_interface(
        bus, path, interface, _vtable, this),
    _intf(bus.getInterface())
{
}

int VPDKeywordEditor::_callback_WriteKeyword(sd_bus_message* msg, void* context,
                                             sd_bus_error* error)
{
    using sdbusplus::server::binding::details::convertForMessage;

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

        std::string path{};
        std::string recordName{};
        std::string keyword{};
        std::vector<uint8_t> value{};

        m.read(path, recordName, keyword, value);

        auto o = static_cast<VPDKeywordEditor*>(context);
        o->writeKeyword(path, recordName, keyword, value);

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
namespace VPDKeywordEditor
{
static const auto _param_WriteKeyword = utility::tuple_to_array(
    message::types::type_id<std::string, std::string, std::string,
                            std::vector<uint8_t>>());
static const auto _return_WriteKeyword =
    utility::tuple_to_array(std::make_tuple('\0'));
} // namespace VPDKeywordEditor
} // namespace details

const vtable::vtable_t VPDKeywordEditor::_vtable[] = {
    vtable::start(),

    vtable::method("WriteKeyword",
                   details::VPDKeywordEditor::_param_WriteKeyword.data(),
                   details::VPDKeywordEditor::_return_WriteKeyword.data(),
                   _callback_WriteKeyword),
    vtable::end()};

} // namespace server
} // namespace VPD
} // namespace Inventory
} // namespace openbmc_project
} // namespace xyz
} // namespace sdbusplus
