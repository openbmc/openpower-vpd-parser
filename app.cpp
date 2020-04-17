#include "args.hpp"
#include "defines.hpp"
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
        args::Args arguments = args::parse(argc, argv);

	bool have_vpd = arguments.count("vpd");
	bool do_dump = arguments.count("dump");
	bool do_fru = arguments.count("fru") && arguments.count("object");

	if (!have_vpd) {
		std::cerr << "VPD file required (--vpd=<filename>)\n";
		return -1;
	}

	if (!do_dump && !do_fru) {
            std::cerr << "No task to perform\n\n";
            std::cerr << "  Update FRU: --fru <type> --object <path>\n";
            std::cerr << "              --fru <type1>,<type2> --object <path1>,<path2>\n\n";
            std::cerr << "  Dump VPD: --dump\n\n";
	    return -1;
	}

	// Read binary VPD file
	auto file = arguments.at("vpd")[0];
	std::ifstream vpdFile(file, std::ios::binary);
	Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
			std::istreambuf_iterator<char>());

	// Parse VPD
	auto vpdStore = parse(std::move(vpd));

	if (do_dump)
	{
	    vpdStore.dump();
	}

        // Set FRU based on FRU type and object path
        if (do_fru)
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
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    return rc;
}
