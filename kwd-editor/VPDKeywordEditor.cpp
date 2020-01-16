#include <chrono>
#include <iostream>
#include "VPDKeywordEditor.hpp"

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
		std::cout <<busName << std::endl;
		std::cout <<objPath << std::endl;
		std::cout <<iFace << std::endl;
		_bus.request_name(busName);
	}

	void VPDKeywordEditor::run() noexcept
	{
		while (true)
		{
			try
			{
				std::cout<<"Test call to DBus event loop"<<std::endl;
				_bus.process_discard();
				_bus.wait((5000000us).count());
			//	_bus.wait();
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}
	}

	void VPDKeywordEditor::writeKeyword( std::string path, std::string recordName, std::string keyword,  std::vector<uint8_t> value)
	{
		std::cout<<"Test msg... Implementation pending"<<std::endl;
	}
}
}
}
}
