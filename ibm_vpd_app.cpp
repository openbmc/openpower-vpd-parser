#include "CLI/CLI.hpp"
#include "defines.hpp"
#include "ibm_vpd_type_check.hpp"
#include "keyword_vpd_parser.hpp"
#include "parser.hpp"
#include "write.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace std;

int main(int argc, char** argv)
{
    int rc = 0;

    try
    {
        using namespace CLI;
        using namespace openpower::vpd;
        using namespace vpd::keyword::parser;

        App app{"IBM VPD"};
        string file{};
        string objectPath{};

        app.add_option("-f, --file", file,
                       "File containing IBM VPD (IPZ/KEYWORD/BONO)")
            ->required()
            ->check(ExistingFile);

        CLI11_PARSE(app, argc, argv);

        // Open the file in binary mode
        ifstream ibmVpdFile(file, ios::binary);
        // Read the content of the binary file into a vector
        Binary ibmVpdVector((istreambuf_iterator<char>(ibmVpdFile)),
                            istreambuf_iterator<char>());

        int ibmVpdType = ibmVpdTypeCheck(ibmVpdVector);
        if (ibmVpdType == 0)
        {
            // Invoking IPZ Vpd Parser
            auto vpdStore = parse(move(ibmVpdVector));
        }
        else if (ibmVpdTypeCheck(ibmVpdVector) == 1)
        {
            // Creating Keyword Vpd Parser Object
            KeywordVpdParser parserObj(move(ibmVpdVector));
            // Invoking KW Vpd Parser
            KeywordVpdMap kwValMap = parserObj.parseKwVpd();

#ifdef DEBUG_KW_VPD
            cerr << '\n' << " KW " << '\t' << "  VALUE " << '\n';
            for (auto it = kwValMap.begin(); it != kwValMap.end(); ++it)
            {
                cerr << '\n' << " " << it->first << '\t';
                copy((it->second).begin(), (it->second).end(),
                     ostream_iterator<int>(cout << hex, " "));
            }
#endif
        }
        else
        {
            cerr << "Invalid IBM VPD format";
        }
    }
    catch (exception& e)
    {
        cerr << e.what() << "\n";
        rc = -1;
    }

    return rc;
}
