#include "config.h"

#include "vpd_keyword_editor.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <sdbusplus/bus.hpp>

int main(int argc, char* argv[])
{
    try
    {
        openpower::vpd::keyword::editor::VPDKeywordEditor KeywordEditor(
            sdbusplus::bus::new_system(), BUSNAME, OBJPATH, IFACE);
        KeywordEditor.run();
        exit(EXIT_SUCCESS);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
    exit(EXIT_FAILURE);
}
