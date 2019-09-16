#include "keyword_vpd_main.hpp"

#include "CLI/CLI.hpp"
#include "keyword_vpd_parser.hpp"

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

    std::string fileName{};
    app.add_option("-f, --file", fileName)
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    // Open the file in binary mode
    std::ifstream keywordVpdFile(fileName, std::ios::binary);

    // Read the content of the binary file into a vector
    binary keywordVpdVector((std::istreambuf_iterator<char>(keywordVpdFile)),
                            std::istreambuf_iterator<char>());

    // Creating Parser Object
    KeywordVpdParser parserObj(std::move(keywordVpdVector));

    try
    {
        auto kwValMap = parserObj.kwVpdParser();

#ifdef DEBUG
        std::cerr << '\n' << " KW " << '\t' << "  VALUE " << '\n';
        for (auto it = kwValMap.begin(); it != kwValMap.end(); ++it)
        {
            std::cerr << '\n' << " " << it->first << '\t';
            std::copy((it->second).begin(), (it->second).end(),
                      std::ostream_iterator<int>(std::cout << std::hex, " "));
        }
#endif
    }
    catch (...)
    {
        std::cerr << "Caught run time exception from keyword VPD parser";
    }
}
