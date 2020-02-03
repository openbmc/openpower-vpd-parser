#include "config.h"

#include "vpd_keyword_editor.hpp"

#include "parser.hpp"

#include <exception>
#include <iostream>
#include <vector>

namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{
VPDKeywordEditor::VPDKeywordEditor(sdbusplus::bus::bus&& bus,
                                   const char* busName, const char* objPath,
                                   const char* iFace) :
    ServerObject<EditorIface>(bus, objPath),
    _bus(std::move(bus)), _manager(_bus, objPath)
{
    _bus.request_name(busName);
}

void VPDKeywordEditor::run()
{
    while (true)
    {
        try
        {
            _bus.process_discard();
            // wait for event
            _bus.wait();
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}

void VPDKeywordEditor::writeKeyword(const std::string inventoryPath,
                                    const std::string recordName,
                                    const std::string keyword,
                                    const Binary value)
{
    // implements the interface to write keyword VPD data
}

} // namespace editor
} // namespace keyword
} // namespace vpd
} // namespace openpower
