#include <getopt.h>
#include <string>
#include <utility>
#include <iostream>
#include "args.hpp"

namespace openpower
{
namespace vpd
{
namespace args
{

static constexpr auto shortForm = "v:f:o:h";
static const option longForm[] =
{
    { "vpd",     required_argument,  nullptr,  'v' },
    { "fru",     required_argument,  nullptr,  'f' },
    { "object",  required_argument,  nullptr,  'o' },
    { "help",    no_argument,        nullptr,  'h' },
    { 0, 0, 0, 0},
};

void usage(char** argv)
{
    std::cerr << std::endl;
    std::cerr << "Usage: " << argv[0] << " args" << std::endl;
    std::cerr << "args:" << std::endl;
    std::cerr << "--vpd=<vpd file> pathname of file containing vpd,";
    std::cerr << " for eg an eeprom file" << std::endl;
    std::cerr << "--fru=<FRU type>, supported types:" << std::endl;
    std::cerr << "\t" << "bmc" << std::endl;
    std::cerr << "\t" << "ethernet" << std::endl;
    std::cerr << "--object=<FRU object path> for eg,";
    std::cerr << " chassis/bmc0/planar" << std::endl;
    std::cerr << "--help display usage" << std::endl;
    std::cerr << std::endl;
}

Args parse(int argc, char** argv)
{
    Args args;
    int option = 0;
    if(1 == argc)
    {
        usage(argv);
    }
    while(-1 !=
        (option = getopt_long(argc, argv, shortForm, longForm, nullptr)))
    {
        if(('h' == option) || ('?' == option))
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
            if((no_argument != which->has_arg) && optarg)
            {
                args.emplace(std::make_pair(which->name,
                                            std::string(optarg)));
            }
        }
    }

    return args;
}

} // namespace args
} // namespace vpd
} // namespace openpower
