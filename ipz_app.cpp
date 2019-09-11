#include "CLI/CLI.hpp"
#include "defines.hpp"
#include "parser.hpp"
#include "write.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace std;

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        using namespace CLI;
        using namespace openpower::vpd;

        App app{"ipz-read-vpd - App to read IPZ format VPD, parse it and store "
                "in DBUS"};
        string file{};

        app.add_option("-f, --file", file, "File containing VPD in IPZ format")
            ->required()
            ->check(ExistingFile);

        CLI11_PARSE(app, argc, argv);

        ifstream vpdFile(file, ios::binary);
        Binary vpd((istreambuf_iterator<char>(vpdFile)),
                   istreambuf_iterator<char>());

        // Use ipz vpd Parser
        auto vpdStore = parse(move(vpd));

        // Write it to D-bus
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
    }

    return rc;
}
