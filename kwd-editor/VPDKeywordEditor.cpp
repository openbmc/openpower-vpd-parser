#include <chrono>
#include <iostream>
#include "VPDKeywordEditor.hpp"
#include "parser.hpp"
#include <fstream>
#include <exception>
#include <nlohmann/json.hpp>
#include <vector>
#include "config.h"

using namespace std::literals::chrono_literals;
namespace openpower
{
namespace vpd
{
namespace keyword
{
namespace editor
{
	VPDKeywordEditor::VPDKeywordEditor(sdbusplus::bus::bus&& bus, const char* busName, const char* objPath, const char* iFace)
	:ServerObject<kwdEditorIface>(bus, objPath), _bus(std::move(bus)), _manager(_bus, objPath)
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
				//wait for event
				_bus.wait();
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}
	}


	void VPDKeywordEditor::writeKeyword( std::string inventoryPath, std::string recordName, std::string keyword,  std::vector<uint8_t> value)
	{
		//implements the interface to write keyword VPD data
	}

} //namespace editor
} //namespace keyword
} //namespace vpd
} //namespace openpower
