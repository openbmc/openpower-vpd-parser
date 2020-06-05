#include "writeEE.hpp"

#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <CLI/CLI.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

static const auto EEPROM_JSON =
    "/usr/share/openpower-fru-vpd/write-eeprom.json";

using json = nlohmann::json;
using namespace CLI;

int main(int argc, char** argv)
{
    int rc = 0;

    // Parse the json file.
    parseJsonFromFile(EEPROM_JSON);

    App app{"VPD Command line tool to update VPD data in eeprom."};

    auto val = app.add_option("--value, -v", value, "Enter the data");

    auto writePN_Flag =
        app.add_flag("--partNumber, -p",
                     "Update the value of PartNumber. {writeEE -p -v PNdata}")
            ->needs(val);

    auto writeSN_Flag =
        app.add_flag("--serialNumber, -s",
                     "Update the value of SerialNumber. {writeEE -s -v SNdata}")
            ->needs(val);

    auto writePTN_Flag =
        app.add_flag("--prettyName, -d",
                     "Update the value of PrettyName. {writeEE -d -v 'PTNdata'}"
                     "ex. writeEE -d -v 'SYSTEM PLANAR' ")
            ->needs(val);

    auto writeMAC_Flag =
        app.add_flag("--mac0, -m",
                     "Update the value of MAC0. {writeEE -m -v PTNdata}"
                     "ex. writeEE -m -v 0894ef80a13d")
            ->needs(val);

    auto writeMAC1_Flag =
        app.add_flag("--mac1, -n",
                     "Update the value of MAC1. {writeEE -n -v PTNdata}"
                     "ex. writeEE -n -v 0894ef80a13e")
            ->needs(val);

    CLI11_PARSE(app, argc, argv);

    if (efile)
    {
        try
        {
            if (*writePN_Flag)
            {
                rwPartNumber(getDataAddr(static_cast<int>(vpdData::VP)), value);
            }
            else if (*writeSN_Flag)
            {
                rwSerialNumber(getDataAddr(static_cast<int>(vpdData::VS)),
                               value);
            }
            else if (*writePTN_Flag)
            {
                rwPrettyName(getDataAddr(static_cast<int>(vpdData::DR)), value);
            }
            else if (*writeMAC_Flag)
            {
                rwMAC(getDataAddr(static_cast<int>(vpdData::B1)), value);
            }
            else if (*writeMAC1_Flag)
            {
                rwMAC(getDataAddr(static_cast<int>(vpdData::B1_1)), value);
            }
        }
        catch (exception& e)
        {
            cerr << e.what();
        }
    }

    efile.close();

    return rc;
}

// Check the addr of data
int getDataAddr(int keyword)
{
    int i, j, len;
    char* data;

    efile.open(const_cast<char*>(eepromPATH.c_str()),
               ios::in | ios::out | ios::binary);
    if (!efile)
    {
        std::cerr << "open the file of eeprom error!" << std::endl;
        efile.close();
    }

    // Get the length of eeprom.
    efile.seekg(0, ios::end);
    len = efile.tellg();
    efile.seekg(0, ios::beg);

    data = new char[len];
    efile.read(data, len);

    vpdData code;
    code = static_cast<vpdData>(keyword);
    switch (code)
    {
        case vpdData::VP:
            for (i = 0; i < len; i++)
            {
                for (j = 0; j < len; j++)
                {
                    if (data[i] == 'O' && data[i + 1] == 'P' &&
                        data[i + 2] == 'F' && data[i + 3] == 'R')
                    {
                        if (data[j] == 'V' && data[j + 1] == 'P' && j > i)
                        {
                            return j + 3;
                        }
                    }
                }
            }
            break;
        case vpdData::VS:
            for (i = 0; i < len; i++)
            {
                for (j = 0; j < len; j++)
                {
                    if (data[i] == 'O' && data[i + 1] == 'P' &&
                        data[i + 2] == 'F' && data[i + 3] == 'R')
                    {
                        if (data[j] == 'V' && data[j + 1] == 'S' && j > i)
                        {
                            return j + 3;
                        }
                    }
                }
            }
            break;
        case vpdData::DR:
            for (i = 0; i < len; i++)
            {
                for (j = 0; j < len; j++)
                {
                    if (data[i] == 'O' && data[i + 1] == 'P' &&
                        data[i + 2] == 'F' && data[i + 3] == 'R')
                    {
                        if (data[j] == 'D' && data[j + 1] == 'R' && j > i)
                        {
                            return j + 3;
                        }
                    }
                }
            }
            break;
        case vpdData::B1:
            for (i = 0; i < len; i++)
            {
                for (j = 0; j < len; j++)
                {
                    if (data[i] == 'R' && data[i + 1] == 'T' &&
                        data[i + 3] == 'V' && data[i + 4] == 'I' &&
                        data[i + 5] == 'N' && data[i + 6] == 'I')
                    {
                        if (data[j] == 'B' && data[j + 1] == '1' && j > i)
                        {
                            return j + 3;
                        }
                    }
                }
            }
            break;
        case vpdData::B1_1:
            for (i = 0; i < len; i++)
            {
                for (j = 0; j < len; j++)
                {
                    if (data[i] == 'R' && data[i + 1] == 'T' &&
                        data[i + 3] == 'O' && data[i + 4] == 'P' &&
                        data[i + 5] == 'F' && data[i + 6] == 'R')
                    {
                        if (data[j] == 'B' && data[j + 1] == '1' && j > i)
                        {
                            return j + 3;
                        }
                    }
                }
            }
            break;
        default:
            break;
    }

    efile.close();

    return 0;
}

// Write the data of PrettyName.
void rwPrettyName(int addr, string data)
{
    int len = 16;
    char* bufw = const_cast<char*>(data.c_str());
    std::cout << "The data of PrettyName is : " << data.c_str() << std::endl;

    if (!efile.is_open())
    {
        efile.open(const_cast<char*>(eepromPATH.c_str()),
                   ios::in | ios::out | ios::binary);
    }

    // Write data.
    efile.seekp(addr, ios::beg);
    efile.write(bufw, len);

    efile.close();
}

// Write the data of PartNumber.
void rwPartNumber(int addr, string data)
{
    int len = 16;
    char* bufw = const_cast<char*>(data.c_str());
    std::cout << "The data of PartNumber is : " << data.c_str() << std::endl;

    if (!efile.is_open())
    {
        efile.open(const_cast<char*>(eepromPATH.c_str()),
                   ios::in | ios::out | ios::binary);
    }

    efile.seekp(addr, ios::beg);
    efile.write(bufw, len);

    efile.close();
}

// Write the data of SerialNumber.
void rwSerialNumber(int addr, string data)
{
    int len = 16;
    char* bufw = const_cast<char*>(data.c_str());
    std::cout << "The data of SerialNumber is : " << data.c_str() << std::endl;

    if (!efile.is_open())
    {
        efile.open(const_cast<char*>(eepromPATH.c_str()),
                   ios::in | ios::out | ios::binary);
    }

    efile.seekp(addr, ios::beg);
    efile.write(bufw, len);

    efile.close();
}

// Write the data of MAC.
// Separate the string to six part
// and write them to eeprom separately.
void rwMAC(int addr, string data)
{
    // Separate the string to six part.
    string hvalue0 = data.substr(0, 2);
    string hvalue1 = data.substr(2, 2);
    string hvalue2 = data.substr(4, 2);
    string hvalue3 = data.substr(6, 2);
    string hvalue4 = data.substr(8, 2);
    string hvalue5 = data.substr(10, 2);

    char bufw0[3];
    char bufw1[3];
    char bufw2[3];
    char bufw3[3];
    char bufw4[3];
    char bufw5[3];

    strcpy(bufw0, hexToASCII(hvalue0).c_str());
    strcpy(bufw1, hexToASCII(hvalue1).c_str());
    strcpy(bufw2, hexToASCII(hvalue2).c_str());
    strcpy(bufw3, hexToASCII(hvalue3).c_str());
    strcpy(bufw4, hexToASCII(hvalue4).c_str());
    strcpy(bufw5, hexToASCII(hvalue5).c_str());

    if (!efile.is_open())
    {
        efile.open(const_cast<char*>(eepromPATH.c_str()),
                   ios::in | ios::out | ios::binary);
    }

    efile.seekp(addr, ios::beg);
    efile.write(bufw0, 2);

    efile.seekp(addr + 1, ios::beg);
    efile.write(bufw1, 2);

    efile.seekp(addr + 2, ios::beg);
    efile.write(bufw2, 2);

    efile.seekp(addr + 3, ios::beg);
    efile.write(bufw3, 2);

    efile.seekp(addr + 4, ios::beg);
    efile.write(bufw4, 2);

    efile.seekp(addr + 5, ios::beg);
    efile.write(bufw5, 2);

    efile.close();
}

// hex-to-ascii c++
string hexToASCII(string hex)
{
    // Initialize the ASCII code string as empty.
    string ascii_value = "";
    for (size_t i = 0; i < hex.length(); i += 2)
    {
        // Extract two characters from hex string.
        string part_value = hex.substr(i, 2);

        // Change it into base 16 and typecast as the character.
        char ch = std::stoul(part_value, nullptr, 16);

        // Add this char to final ASCII string.
        ascii_value += ch;
    }
    return ascii_value;
}

// Parse the json file.
void parseJsonFromFile(const char* filename)
{
    ifstream jfile;
    jfile.open(filename);
    if (!jfile.is_open())
    {
        std::cerr << "Can't open json file." << std::endl;
    }

    auto jfdata = json::parse(jfile, nullptr, false);
    if (jfdata.is_discarded())
    {
        std::cerr << "Parser json file fail." << std::endl;
    }

    static const std::vector<json> empty{};
    std::vector<json> reading = jfdata.value("frus", empty);
    if (!reading.empty())
    {
        for (const auto& instance : reading)
        {
            eepromPATH = instance.value("PATH", "");
            std::cerr << "eepromPATH = " << eepromPATH << std::endl;
        }
    }
    else
    {
        std::cerr << "Can't get the VPDdata" << std::endl;
    }

    jfile.close();
}
