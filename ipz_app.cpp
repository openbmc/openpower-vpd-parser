#include "CLI/CLI.hpp"
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
        using namespace CLI;
        using namespace openpower::vpd;

        // Get the input binary file using CLI
        CLI::App app{"ipz-read-vpd"};
        std::string file{};

        app.add_option("-f, --file", file)
            ->required()
            ->check(CLI::ExistingFile);

        CLI11_PARSE(app, argc, argv);

        std::ifstream vpdFile(file, std::ios::binary);
        Binary vpd((std::istreambuf_iterator<char>(vpdFile)),
                   std::istreambuf_iterator<char>());

        // Use ipz vpd Parser
        auto vpdStore = parse(std::move(vpd));

        // Write it to D-bus
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }

    return rc;
}
