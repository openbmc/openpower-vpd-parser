#include "keyword_vpd_main.hpp"

#include "CLI/CLI.hpp"
#include "keyword_vpd_parser.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace CLI;

int main(int argc, char** argv)
{
    using namespace openpower::keywordVpd;

    // Get the input binary file using CLI
    CLI::App app{"Keyword VPD Parser"};

    std::string fileName;
    app.add_option("-f, --file", fileName);

    CLI11_PARSE(app, argc, argv);

    // Open the file in binary mode
    std::ifstream keywordVpdFile(fileName, std::ios::binary);

    if (keywordVpdFile)
    {
        // Read the content of the binary file into a vector
        openpower::keywordVpd::parser::Binary keywordVpdVector(
            (std::istreambuf_iterator<char>(keywordVpdFile)),
            std::istreambuf_iterator<char>());

        // Creating Parser Object
        openpower::keywordVpd::parser::KeywordVpdParser parserObj(
            keywordVpdVector);

        // Storing kw-val pairs in map by calling the Parser function
        std::unordered_map<string, vector<uint8_t>> kwValMap =
            parserObj.kwVpdParser();

        for (auto it = kwValMap.begin(); it != kwValMap.end(); ++it)
        {
            std::cerr << '\n' << "KW: " << it->first << '\t';
            std::copy((it->second).begin(), (it->second).end(),
                      std::ostream_iterator<int>(std::cout << std::hex, " "));
        }
    }
    else
    {
        cerr << "\nInvalid file" << endl;
    }
}
