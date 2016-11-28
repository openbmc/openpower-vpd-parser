#include <defines.hpp>
#include <parser.hpp>
#include <iostream>
#include <string>
#include <iterator>
#include <fstream>
#include <write.hpp>
#include "args.hpp"

int main(int argc, char** argv)
{
    int rc = 0;

    using namespace openpower::vpd;

    args::Args arguments =  args::parse(argc, argv);

    // We need vpd file, FRU type and object path
    if((arguments.end() != arguments.find("vpd")) &&
       (arguments.end() != arguments.find("fru")) &&
       (arguments.end() != arguments.find("object")))
    {
        // Read binary VPD file
        auto file = arguments.at("vpd");
        std::ifstream vpdFile(file, std::ios::binary);
        Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
                    std::istreambuf_iterator<char>());

        // Parse vpd
        auto vpdStore = parse(std::move(vpd));

        // Write VPD to FRU inventory
        inventory::write(
            arguments.at("fru"),
            std::move(vpdStore),
            arguments.at("object"));
    }
    else
    {
        std::cerr << "Need VPD file, FRU type and object path" << std::endl;
        rc = -1;
    }

    return rc;
}
