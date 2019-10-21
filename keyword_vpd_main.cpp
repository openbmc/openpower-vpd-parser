#include "CLI/CLI.hpp"
#include "keyword_vpd_parser.hpp"
#include "keyword_vpd_types.hpp"
#include "utils.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;
using namespace CLI;
using namespace vpd::keyword::parser;
using namespace openpower::vpd;

void vpd::keyword::parser::kwVpdDbusObj(KeywordVpdMap kwValMap)
{
    inventory::InterfaceMap interfaces;
    inventory::ObjectMap objects;
    inventory::PropertyMap propMap;
    string objectPath{};
    sdbusplus::message::object_path object(objectPath);
    string interfStr = " ";

    for (const auto& kw : kwValMap)
    {
        propMap.emplace(kw.first, kw.second);
    }

    interfaces.emplace(move(interfStr), move(propMap));
    objects.emplace(move(object), move(interfaces));

    // Notify method call
    inventory::callPIM(move(objects));
}

int main(int argc, char** argv)
{
    // Get the input binary file using CLI
    App app{"Keyword VPD Parser"};

    string fileName{};

    // The keyword vpd file is taken from the command line using -f/--file
    // flags.
    app.add_option("-f, --file", fileName)->required()->check(ExistingFile);

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

        kwVpdDbusObj(kwValMap);
    }
    catch (exception& e)
    {
        cerr << "Run time exception from keyword VPD parser: " << e.what()
             << endl;
    }
}
