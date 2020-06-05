#pragma once
#include <fstream>

using namespace std;

/**
 * The parameter which is used by fstream().
 * - the file of eeprom.
 */
fstream efile;

/**
 * The parameter which is used by CLI.
 * - the VPD data.
 */
string value;

/**
 * The parameter which is used by parseJsonFromFile().
 * - the path of eeprom.
 */
string eepromPATH;

/**
 * Five kinds of VPD data, used by getDataAddr()
 * to get addr of VPD data.
 */
enum class vpdData : int
{
    VP,
    VS,
    DR,
    B1,
    B1_1
};

/**
 * Write "PrettyName" to the binary file of eeprom.
 * @param[in] addr - the address which will be written in eeprom.
 * @param[in] data - the data which will be written in eeprom.
 */
void rwPrettyName(int addr, string data);

/**
 * Write "PartNumber" to the binary file of eeprom.
 * @param[in] addr - the address which will be written in eeprom.
 * @param[in] data - the data which will be written in eeprom.
 */
void rwPartNumber(int addr, string data);

/**
 * Write "SerialNumber" to the binary file of eeprom.
 * @param[in] addr - the address which will be written in eeprom.
 * @param[in] data - the data which will be written in eeprom.
 */
void rwSerialNumber(int addr, string data);

/**
 * Write "MAC" to the binary file of eeprom.
 * @param[in] addr - the address which will be written in eeprom.
 * @param[in] data - the data which will be written in eeprom.
 */
void rwMAC(int addr, string data);

/**
 * Transfer the hex to ASCII.
 * @param[in] hex - the hex which will be transfered to the ASCII.
 * @return string - the ASCII data.
 */
string hexToASCII(string hex);

/**
 * Paser the json file.
 * @param[in] filename - the Json file.
 */
void parseJsonFromFile(const char* filename);

/**
 * Get the data's addr of eeprom.
 * @param[in] keyword - the VPD data(ex. PartNumber, SerialNumber,
 * PrettyName, MAC).
 * @return int - the data's addr of eeprom.
 */
int getDataAddr(int keyword);
