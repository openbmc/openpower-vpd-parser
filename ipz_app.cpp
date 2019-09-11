#include "defines.hpp"
#include "ipz_args.hpp"
#include "parser.hpp"
#include "write.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        using namespace openpower::vpd;
        using namespace ipz::vpd;

        ipz::vpd::args::Args arguments = ipz::vpd::args::parse(argc, argv);

        // We need vpd file, FRU type and object path
        if ((arguments.end() != arguments.find("vpd")) &&
            (arguments.end() != arguments.find("fru")) &&
            (arguments.end() != arguments.find("object")))
        {
            // Read binary VPD file
            auto file = arguments.at("vpd")[0];
            std::ifstream vpdFile(file, std::ios::binary);
            Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
                       std::istreambuf_iterator<char>());

            // Use ipz vpd Parser
            auto vpdStore = parse(std::move(vpd));
        }
        else
        {
            std::cerr << "Need VPD file, FRU type and object path\n";
            rc = -1;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    return rc;
}
