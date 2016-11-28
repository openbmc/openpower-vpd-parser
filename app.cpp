#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <exception>
#include "defines.hpp"
#include "write.hpp"
#include "args.hpp"
#include "parser.hpp"

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        using namespace openpower::vpd;

        args::Args arguments =  args::parse(argc, argv);

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

            // Parse vpd
            auto vpdStore = parse(std::move(vpd));

            using argList = std::vector<std::string>;
            argList frus = std::move(arguments.at("fru"));
            argList objects = std::move(arguments.at("object"));

            if (frus.size() != objects.size())
            {
                std::cerr << "Unequal number of FRU types and object paths "
                          "specified\n";
                rc = -1;
            }
            else
            {
                // Write VPD to FRU inventory
                for (std::size_t index = 0; index < frus.size(); ++index)
                {
                    inventory::write(
                        frus[index],
                        vpdStore,
                        objects[index]);
                }
            }
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
