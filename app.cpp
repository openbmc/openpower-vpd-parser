#include "args.hpp"
#include "defines.hpp"
#include "ipz_parser.hpp"
#include "write.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <variant>
#include <vector>

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        using namespace openpower::vpd;
        using namespace openpower::vpd::ipz::parser;

        args::Args arguments = args::parse(argc, argv);

        bool haveVpd = arguments.count("vpd");
        bool doDump = arguments.count("dump");
        bool doFru = arguments.count("fru") && arguments.count("object");

        if (!haveVpd)
        {
            std::cerr << "VPD file required (--vpd=<filename>)\n";
            return -1;
        }

        if (!doDump && !doFru)
        {
            std::cerr << "No task to perform\n\n";
            std::cerr << "  Update FRU: --fru <type> --object <path>\n";
            std::cerr << "              --fru <t1>,<t2> --object <p1>,<p2>\n\n";
            std::cerr << "  Dump VPD: --dump\n\n";
            return -1;
        }

        // Read binary VPD file
        auto file = arguments.at("vpd")[0];
        std::ifstream vpdFile(file, std::ios::binary);
        Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
                   std::istreambuf_iterator<char>());

        // Parse VPD
        auto vpdStartOffset = 0;
        IpzVpdParser ipzParser(std::move(vpd), std::string{}, file,
                               vpdStartOffset);
        auto vpdStore = std::move(std::get<Store>(ipzParser.parse()));

        if (doDump)
        {
            vpdStore.dump();
        }

        // Set FRU based on FRU type and object path
        if (doFru)
        {
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
                    inventory::write(frus[index], vpdStore, objects[index]);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    return rc;
}
