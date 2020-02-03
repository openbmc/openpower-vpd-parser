#pragma once

#include "types.hpp"

#include <com/ibm/vpd/Editor/server.hpp>
#include <sdbusplus/server.hpp>

namespace sdbusplus
{
namespace bus
{
class bus;
}
} // namespace sdbusplus

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{

template <typename T>
using ServerObject = T;

using EditorIface = sdbusplus::com::ibm::vpd::server::Editor;

/** @class VPDKeywordEditor
 *  @brief OpenBMC keyword editor implementation.
 *
 *  A concrete implementation for the
 *  com.ibm.vpd.Editor
 */
class VPDKeywordEditor : public ServerObject<EditorIface>
{
  public:
    /* Define all of the basic class operations:
     * Not allowed:
     * - Default constructor to avoid nullptrs.
     * - Copy operations due to internal unique_ptr.
     * - Move operations due to 'this' being registered as the
     *  'context' with sdbus.
     * Allowed:
     * - Destructor.
     */
    VPDKeywordEditor() = delete;
    VPDKeywordEditor(const VPDKeywordEditor&) = delete;
    VPDKeywordEditor& operator=(const VPDKeywordEditor&) = delete;
    VPDKeywordEditor(VPDKeywordEditor&&) = delete;
    ~VPDKeywordEditor() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus connection.
     *  @param[in] busName - Name to be requested on Bus
     *  @param[in] objPath - Path to attach at.
     *  @param[in] iFace - interface to implement
     */
    VPDKeywordEditor(sdbusplus::bus::bus&& bus, const char* busName,
                     const char* objPath, const char* iFace);

    /** @brief Implementation for WriteKeyword
     *  Api to update the keyword value for a given inventory.
     *
     *  @param[in] path - Object path of the inventory
     *  @param[in] recordName - name of the record for which the keyword value
     *  has to be modified
     *  @param[in] keyword - keyword whose value needs to be updated
     *  @param[in] value - value that needs to be updated
     */
    void writeKeyword(const std::string path, const std::string recordName,
                      const std::string keyword, const Binary value);

    /** @brief Start processing DBus messages. */
    void run();

  private:
    /** @brief Persistent sdbusplus DBus bus connection. */
    sdbusplus::bus::bus _bus;

    /** @brief sdbusplus org.freedesktop.DBus.ObjectManager reference. */
    sdbusplus::server::manager::manager _manager;
};

} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
