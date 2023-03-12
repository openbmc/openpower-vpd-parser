#include "tool_constants.hpp"
#include "vpd_tool.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <iostream>

/**
 * @brief API to perform manufacturing clean.
 *
 * @param[in] i_mfgCleanConfirmFlag - Confirmation flag to perform manufacturing
 * clean.
 *
 * @return Status returned by cleanSystemVpd operation, success otherwise.
 */
int doMfgClean(const auto& i_mfgCleanConfirmFlag)
{
    if (i_mfgCleanConfirmFlag->empty())
    {
        constexpr auto MAX_CONFIRMATION_STR_LENGTH{3};
        std::string l_confirmation{};
        std::cout
            << "This option resets some of the system VPD keywords to their default values. Do you really wish to proceed further?[yes/no]:";
        std::cin >> std::setw(MAX_CONFIRMATION_STR_LENGTH) >> l_confirmation;

        if (l_confirmation != "yes")
        {
            return vpd::constants::SUCCESS;
        }
    }

    vpd::VpdTool l_vpdToolObj;
    return l_vpdToolObj.cleanSystemVpd();
}

/**
 * @brief API to write keyword's value.
 *
 * @param[in] i_hardwareFlag - Flag to perform write on hardware.
 * @param[in] i_keywordValueOption - Option to read keyword value from command.
 * @param[in] i_vpdPath - DBus object path or EEPROM path.
 * @param[in] i_recordName - Record to be updated.
 * @param[in] i_keywordName - Keyword to be updated.
 * @param[in] i_keywordValue - Value to be updated in keyword.
 *
 * @return Status of writeKeyword operation, failure otherwise.
 */
int writeKeyword(const auto& i_hardwareFlag, const auto& i_keywordValueOption,
                 const std::string& i_vpdPath, const std::string& i_recordName,
                 const std::string& i_keywordName,
                 const std::string& i_keywordValue)
{
    std::error_code l_ec;

    if (!i_hardwareFlag->empty() && !std::filesystem::exists(i_vpdPath, l_ec))
    {
        std::cerr << "Given EEPROM file path doesn't exist : " + i_vpdPath
                  << std::endl;
        return vpd::constants::FAILURE;
    }

    if (l_ec)
    {
        std::cerr << "filesystem call exists failed for file: " << i_vpdPath
                  << ", reason: " + l_ec.message() << std::endl;
        return vpd::constants::FAILURE;
    }

    if (!i_keywordValueOption->empty() && i_keywordValue.empty())
    {
        std::cerr
            << "Please provide keyword value.\nUse --value/--file to give "
               "keyword value. Refer --help."
            << std::endl;
        return vpd::constants::FAILURE;
    }

    if (i_keywordValueOption->empty())
    {
        std::cerr
            << "Please provide keyword value.\nUse --value/--file to give "
               "keyword value. Refer --help."
            << std::endl;
        return vpd::constants::FAILURE;
    }

    vpd::VpdTool l_vpdToolObj;
    return l_vpdToolObj.writeKeyword(i_vpdPath, i_recordName, i_keywordName,
                                     i_keywordValue, !i_hardwareFlag->empty());
}

/**
 * @brief API to read keyword's value.
 *
 * @param[in] i_hardwareFlag - Flag to perform write on hardware.
 * @param[in] i_vpdPath - DBus object path or EEPROM path.
 * @param[in] i_recordName - Record to be updated.
 * @param[in] i_keywordName - Keyword to be updated.
 * @param[in] i_filePath - File path to save keyword's read value.
 *
 * @return Status of readKeyword operation, failure otherwise.
 */
int readKeyword(const auto& i_hardwareFlag, const std::string& i_vpdPath,
                const std::string& i_recordName,
                const std::string& i_keywordName, const std::string& i_filePath)
{
    std::error_code l_ec;

    if (!i_hardwareFlag->empty() && !std::filesystem::exists(i_vpdPath, l_ec))
    {
        std::string l_errMessage{"Given EEPROM file path doesn't exist : " +
                                 i_vpdPath};

        if (l_ec)
        {
            l_errMessage += ". filesystem call exists failed, reason: " +
                            l_ec.message();
        }

        std::cerr << l_errMessage << std::endl;
        return vpd::constants::FAILURE;
    }

    bool l_isHardwareOperation = (!i_hardwareFlag->empty() ? true : false);

    vpd::VpdTool l_vpdToolObj;
    return l_vpdToolObj.readKeyword(i_vpdPath, i_recordName, i_keywordName,
                                    l_isHardwareOperation, i_filePath);
}

/**
 * @brief API to check option value pair in the tool command.
 *
 * In VPD tool command, some of the option(s) mandate values to be passed along
 * with the option. This API based on option, detects those mandatory value(s).
 *
 * @param[in] i_objectOption - Option to pass object path.
 * @param[in] i_vpdPath - Object path, DBus or EEPROM.
 * @param[in] i_recordOption - Option to pass record name.
 * @param[in] i_recordName - Record name.
 * @param[in] i_keywordOption - Option to pass keyword name.
 * @param[in] i_keywordName - Keyword name.
 *
 * @return Success if corresponding value is found against option, failure
 * otherwise.
 */
int checkOptionValuePair(const auto& i_objectOption, const auto& i_vpdPath,
                         const auto& i_recordOption, const auto& i_recordName,
                         const auto& i_keywordOption, const auto& i_keywordName)
{
    if (!i_objectOption->empty() && i_vpdPath.empty())
    {
        std::cout << "Given path is empty." << std::endl;
        return vpd::constants::FAILURE;
    }

    if (!i_recordOption->empty() &&
        (i_recordName.size() != vpd::constants::RECORD_SIZE))
    {
        std::cerr << "Record " << i_recordName << " is not supported."
                  << std::endl;
        return vpd::constants::FAILURE;
    }

    if (!i_keywordOption->empty() &&
        (i_keywordName.size() != vpd::constants::KEYWORD_SIZE))
    {
        std::cerr << "Keyword " << i_keywordName << " is not supported."
                  << std::endl;
        return vpd::constants::FAILURE;
    }

    return vpd::constants::SUCCESS;
}

/**
 * @brief API to create app footer.
 *
 * @param[in] i_app - CLI::App object.
 */
void updateFooter(CLI::App& i_app)
{
    i_app.footer(
        "Read:\n"
        "    IPZ Format:\n"
        "        From DBus to console: "
        "vpd-tool -r -O <DBus Object Path> -R <Record Name> -K <Keyword Name>\n"
        "        From DBus to file: "
        "vpd-tool -r -O <DBus Object Path> -R <Record Name> -K <Keyword Name> --file <File Path>\n"
        "        From hardware to console: "
        "vpd-tool -r -H -O <EEPROM Path> -R <Record Name> -K <Keyword Name>\n"
        "        From hardware to file: "
        "vpd-tool -r -H -O <EEPROM Path> -R <Record Name> -K <Keyword Name> --file <File Path>\n"
        "Write:\n"
        "    IPZ Format:\n"
        "        On DBus: "
        "vpd-tool -w/-u -O <DBus Object Path> -R <Record Name> -K <Keyword Name> -V <Keyword Value>\n"
        "        On DBus, take keyword value from file:\n"
        "              vpd-tool -w/-u -O <DBus Object Path> -R <Record Name> -K <Keyword Name> --file <File Path>\n"
        "        On hardware: "
        "vpd-tool -w/-u -H -O <EEPROM Path> -R <Record Name> -K <Keyword Name> -V <Keyword Value>\n"
        "        On hardware, take keyword value from file:\n"
        "              vpd-tool -w/-u -H -O <EEPROM Path> -R <Record Name> -K <Keyword Name> --file <File Path>\n"
        "Dump Object:\n"
        "    From DBus to console: "
        "vpd-tool -o -O <DBus Object Path>\n"
        "Fix System VPD:\n"
        "    vpd-tool --fixSystemVPD\n"
        "MfgClean:\n"
        "        Flag to clean and reset specific keywords on system VPD to its default value.\n"
        "        vpd-tool --mfgClean\n"
        "Dump Inventory:\n"
        "   From DBus to console in JSON format: "
        "vpd-tool -i\n"
        "   From DBus to console in Table format: "
        "vpd-tool -i -t\n");
}

int main(int argc, char** argv)
{
    CLI::App l_app{"VPD Command Line Tool"};

    std::string l_vpdPath{};
    std::string l_recordName{};
    std::string l_keywordName{};
    std::string l_filePath{};
    std::string l_keywordValue{};

    updateFooter(l_app);

    auto l_objectOption = l_app.add_option("--object, -O", l_vpdPath,
                                           "File path");
    auto l_recordOption = l_app.add_option("--record, -R", l_recordName,
                                           "Record name");
    auto l_keywordOption = l_app.add_option("--keyword, -K", l_keywordName,
                                            "Keyword name");

    // Enable when file option is implemented.
    /*auto l_fileOption = l_app.add_option("--file", l_filePath,
                                         "Absolute file path");*/

    auto l_keywordValueOption =
        l_app.add_option("--value, -V", l_keywordValue,
                         "Keyword value in ascii/hex format."
                         " ascii ex: 01234; hex ex: 0x30313233");

    auto l_hardwareFlag = l_app.add_flag("--Hardware, -H",
                                         "CAUTION: Developer only option.");

    auto l_readFlag = l_app.add_flag("--readKeyword, -r", "Read keyword")
                          ->needs(l_objectOption)
                          ->needs(l_recordOption)
                          ->needs(l_keywordOption);

    auto l_writeFlag =
        l_app
            .add_flag(
                "--writeKeyword, -w,--updateKeyword, -u",
                "Write keyword, Note: Irrespective of DBus or hardware path provided, primary and backup, redundant EEPROM(if any) paths will get updated with given key value")
            ->needs(l_objectOption)
            ->needs(l_recordOption)
            ->needs(l_keywordOption);

    // ToDo: Take offset value from user for hardware path.

    auto l_dumpObjFlag =
        l_app
            .add_flag("--dumpObject, -o",
                      "Dump specific properties of an inventory object")
            ->needs(l_objectOption);

    auto l_fixSystemVpdFlag = l_app.add_flag(
        "--fixSystemVPD",
        "Use this option to interactively fix critical system VPD keywords");
    auto l_dumpInventoryFlag = l_app.add_flag("--dumpInventory, -i",
                                              "Dump all the inventory objects");

    auto l_mfgCleanFlag = l_app.add_flag(
        "--mfgClean", "Manufacturing clean on system VPD keyword");

    auto l_mfgCleanConfirmFlag = l_app.add_flag(
        "--yes", "Using this flag with --mfgClean option, assumes "
                 "yes to proceed without confirmation.");

    auto l_dumpInventoryTableFlag =
        l_app.add_flag("--table, -t", "Dump inventory in table format");

    CLI11_PARSE(l_app, argc, argv);

    if (checkOptionValuePair(l_objectOption, l_vpdPath, l_recordOption,
                             l_recordName, l_keywordOption,
                             l_keywordName) == vpd::constants::FAILURE)
    {
        return vpd::constants::FAILURE;
    }

    if (!l_readFlag->empty())
    {
        return readKeyword(l_hardwareFlag, l_vpdPath, l_recordName,
                           l_keywordName, l_filePath);
    }

    if (!l_writeFlag->empty())
    {
        return writeKeyword(l_hardwareFlag, l_keywordValueOption, l_vpdPath,
                            l_recordName, l_keywordName, l_keywordValue);
    }

    if (!l_dumpObjFlag->empty())
    {
        vpd::VpdTool l_vpdToolObj;
        return l_vpdToolObj.dumpObject(l_vpdPath);
    }

    if (!l_fixSystemVpdFlag->empty())
    {
        vpd::VpdTool l_vpdToolObj;
        return l_vpdToolObj.fixSystemVpd();
    }

    if (!l_mfgCleanFlag->empty())
    {
        return doMfgClean(l_mfgCleanConfirmFlag);
    }

    if (!l_dumpInventoryFlag->empty())
    {
        vpd::VpdTool l_vpdToolObj;
        return l_vpdToolObj.dumpInventory(!l_dumpInventoryTableFlag->empty());
    }

    std::cout << l_app.help() << std::endl;
    return vpd::constants::FAILURE;
}
