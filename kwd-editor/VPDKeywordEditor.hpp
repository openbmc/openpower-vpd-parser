#pragma once

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Inventory/VPD/VPDKeywordEditor/server.hpp>

namespace sdbusplus
{
namespace bus
{
class bus;
}
} // namespace sdbuspluss

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

using kwdEditorIface = sdbusplus::xyz::openbmc_project::Inventory::VPD::server::VPDKeywordEditor;

/** @class VPDKeywordEditor
 * *  @brief OpenBMC keywod editor implementation.
 * *
 * *  A concrete implementation for the xyz.openbmc_project.Inventory.VPD.server.VPDKeywordEditor
 * */
class VPDKeywordEditor:public ServerObject<kwdEditorIface>
{
	public:
        /* Define all of the basic class operations:
 	**     Not allowed:
	**         - Default constructor to avoid nullptrs.
	**         - Copy operations due to internal unique_ptr.
 	**         - Move operations due to 'this' being registered as the
 	**           'context' with sdbus.
 	**     Allowed:
 	**         - Destructor.
 	**/
        VPDKeywordEditor() = delete;
        VPDKeywordEditor(const VPDKeywordEditor&) = delete;
        VPDKeywordEditor& operator=(const VPDKeywordEditor&) = delete;
        VPDKeywordEditor(VPDKeywordEditor&&) = delete;
		
	/** @brief Constructor to put object onto bus at a dbus path.
 	**  @param[in] bus - Bus connection.
 	**  @param[in] busName - Name to be requeted on Bus
 	**  @param[in] objPath - Path to attach at.
 	**  @param[in] iFace - interface to implement
 	**/
        VPDKeywordEditor(sdbusplus::bus::bus&& bus, const char* busName, const char* objPath, const char* iFace);
		
	/** @brief Implementation for WriteKeyword
 	**  Api to update the keyword value in a given VPD record.
 	**
 	**  @param[in] path - path to the VPD record file of the FRU
 	**  @param[in] recordName - name of the record for which the keyword value has to be modified
 	**  @param[in] keyword - keyword whose value needs to be updated
 	**  @param[in] value - value that needs to be updated for the given keyword
 	**/
        void writeKeyword( std::string path, std::string recordName, std::string keyword,
            std::vector<uint8_t> value);
			
	/** @brief Start processing DBus messages. */
	void run() noexcept;

	private:
	/** @brief Persistent sdbusplus DBus bus connection. */
	sdbusplus::bus::bus _bus;

	/** @brief sdbusplus org.freedesktop.DBus.ObjectManager reference. */
	sdbusplus::server::manager::manager _manager;

	//sdbusplus::server::interface::interface _interface;

};

}
}
}
}
