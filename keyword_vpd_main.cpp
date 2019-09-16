#include "CLI11.hpp"
#include "keyword_vpd_parser.hpp"
#include "keyword_vpd_types.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;
using namespace CLI;

int main(int argc, char** argv)
{
    using namespace vpd::keyword::parser;

    // Get the input binary file using CLI
    CLI::App app{"Keyword VPD Parser"};

    string fileName{};
    app.add_option("-f, --file", fileName)
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    // Open the file in binary mode
    ifstream keywordVpdFile(fileName, ios::binary);

    // Read the content of the binary file into a vector
    Binary keywordVpdVector((istreambuf_iterator<char>(keywordVpdFile)),
                            istreambuf_iterator<char>());

    // Creating Parser Object
    KeywordVpdParser parserObj(move(keywordVpdVector));

    try
    {
        KeywordVpdMap kwValMap = parserObj.kwVpdParser();

#ifdef DEBUG
        cerr << '\n' << " KW " << '\t' << "  VALUE " << '\n';
        for (auto it = kwValMap.begin(); it != kwValMap.end(); ++it)
        {
            cerr << '\n' << " " << it->first << '\t';
            copy((it->second).begin(), (it->second).end(),
                 ostream_iterator<int>(cout << hex, " "));
        }
#endif
    }
    catch (exception& e)
    {
        cerr << "Run time exception from keyword VPD parser: " << e.what()
             << endl;
    }
}
