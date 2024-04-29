#include "args.hpp"

#include <getopt.h>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace openpower
{
namespace vpd
{
namespace args
{

static constexpr auto shortForm = "v:f:o:dh";
static const option longForm[] = {
    {"vpd", required_argument, nullptr, 'v'},
    {"fru", required_argument, nullptr, 'f'},
    {"object", required_argument, nullptr, 'o'},
    {"dump", no_argument, nullptr, 'd'},
    {"help", no_argument, nullptr, 'h'},
    {0, 0, 0, 0},
};

void usage(char** argv)
{
    std::cerr << "\nUsage: " << argv[0] << " args\n";
    std::cerr << "args:\n";
    std::cerr << "--vpd=<vpd file> pathname of file containing vpd,";
    std::cerr << " for eg an eeprom file\n";
    std::cerr << "--dump output contents of parsed VPD\n";
    std::cerr << "--fru=<FRU type>, supported types:\n";
    std::cerr << "\t" << "bmc\n";
    std::cerr << "\t" << "ethernet\n";
    std::cerr << "Specify multiple FRU types via comma-separated list\n";
    std::cerr << "--object=<FRU object path> for eg,";
    std::cerr << " chassis/bmc0/planar\n";
    std::cerr << "Specify multiple object paths via comma-separated list, "
                 "ordered as the FRU types\n";
    std::cerr << "--help display usage\n";
}

Args parse(int argc, char** argv)
{
    Args args;
    int option = 0;
    if (1 == argc)
    {
        usage(argv);
    }
    while (-1 !=
           (option = getopt_long(argc, argv, shortForm, longForm, nullptr)))
    {
        if (('h' == option) || ('?' == option))
        {
            usage(argv);
        }
        else
        {
            auto which = &longForm[0];
            // Figure out which option
            while ((which->val != option) && (which->val != 0))
            {
                ++which;
            }
            // If option needs an argument, note the argument value
            if ((no_argument != which->has_arg) && optarg)
            {
                using argList = std::vector<std::string>;
                argList values;
                // There could be a comma-separated list
                std::string opts(optarg);
                std::istringstream stream(std::move(opts));
                std::string input{};
                while (std::getline(stream, input, ','))
                {
                    values.push_back(std::move(input));
                }
                args.emplace(which->name, std::move(values));
            }
            else
            {
                // Add options that do not have arguments to the arglist
                args.emplace(which->name, std::vector<std::string>{});
            }
        }
    }

    return args;
}

} // namespace args
} // namespace vpd
} // namespace openpower
