#include "tool_constants.hpp"
#include "tool_error_codes.hpp"
#include "tool_split_mode.hpp"
#include "tool_utils.hpp"
#include "vpd_tool.hpp"

#include <CLI/CLI.hpp>

#include <filesystem>
#include <iostream>

/**
 * @brief Resets the VPD on DBus for all the Frus.
 *
 * API clears the inventory persisted data and restarts the phosphor inventory
 * manager(PIM) DBus service and the VPD manager service. VPD manager service
 * collects the VPD for all the FRU's listed on the system config JSON and calls
 * PIM to publish VPD on DBus.
 *
 * Note: Force reset only happens if chassis is powered off.
 *
 * @return On success returns 0, otherwise returns -1.
 */
int forceReset()
{
    if (vpd::utils::isChassisPowerOff())
    {
        vpd::VpdTool l_vpdToolObj;
        return l_vpdToolObj.resetVpdOnDbus();
    }

    std::cerr
        << "The chassis power state is not Off. Force reset operation is not allowed."
        << std::endl;
    return vpd::constants::FAILURE;
}

/**
 * @brief API to perform manufacturing clean.
 *
 * @param[in] i_mfgCleanConfirmFlag - Confirmation flag to perform manufacturing
 * clean.
 * @param[in] i_syncBiosAttributesFlag - Flag which specifies whether
 * BIOS attribute related keywords need to be synced from BIOS Config Manager
 * instead of being reset to default value.
 *
 * @return Status returned by cleanSystemVpd operation, success otherwise.
 */
int doMfgClean(const auto& i_mfgCleanConfirmFlag,
               const auto& i_syncBiosAttributesFlag)
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

    // delete the vpd dump directory
    l_vpdToolObj.clearVpdDumpDir();

    return l_vpdToolObj.cleanSystemVpd(!i_syncBiosAttributesFlag->empty());
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
        std::cerr << "Given EEPROM file path doesn't exist[" + i_vpdPath << "]."
                  << std::endl;
        if (l_ec)
        {
            std::cerr << "Reason: " + l_ec.message() << std::endl;
        }
        return static_cast<int>(vpd::ErrorCode::EEPROM_PATH_NOT_FOUND);
    }

    if (!i_keywordValueOption->empty() && i_keywordValue.empty())
    {
        std::cerr
            << "Please provide keyword value.\nUse --value/--file to give "
               "keyword value. Refer --help."
            << std::endl;
        return static_cast<int>(vpd::ErrorCode::KEYWORD_VALUE_NOT_PROVIDED);
    }

    if (i_keywordValueOption->empty())
    {
        std::cerr
            << "Please provide keyword value.\nUse --value/--file to give "
               "keyword value. Refer --help."
            << std::endl;
        return static_cast<int>(vpd::ErrorCode::KEYWORD_VALUE_NOT_PROVIDED);
    }

    if (i_keywordName == vpd::constants::KwdIM)
    {
        if (!(i_keywordValue.substr(0, 2).compare("0x") ==
              vpd::constants::STR_CMP_SUCCESS))
        {
            std::cerr << "Please provide IM value in hex format." << std::endl;
            return static_cast<int>(vpd::ErrorCode::INVALID_INPUT_PARAMETER);
        }

        if (std::find(vpd::constants::validImValues.begin(),
                      vpd::constants::validImValues.end(), i_keywordValue) ==
            vpd::constants::validImValues.end())
        {
            std::cerr << "Given IM value [" << i_keywordValue
                      << "] doesn't match with any of the valid system type."
                      << std::endl;
            return static_cast<int>(vpd::ErrorCode::INVALID_INPUT_PARAMETER);
        }
    }

    vpd::VpdTool l_vpdToolObj;
    return l_vpdToolObj.writeKeyword(i_vpdPath, i_recordName, i_keywordName,
                                     i_keywordValue, !i_hardwareFlag->empty());
}

/**
 * @brief API to read keyword's value.
 *
 * @param[in] i_hardwareFlag - Flag to perform read on hardware.
 * @param[in] i_vpdPath - DBus object path or EEPROM path.
 * @param[in] i_recordName - Record to be read.
 * @param[in] i_keywordName - Keyword to be read.
 * @param[in] i_filePath - File path to save keyword's read value.
 *
 * @return On success return number of bytes read, corresponding error code
 * incase of exception/error.
 */
int readKeyword(const auto& i_hardwareFlag, const std::string& i_vpdPath,
                const std::string& i_recordName,
                const std::string& i_keywordName, const std::string& i_filePath)
{
    std::error_code l_ec;

    if (!i_hardwareFlag->empty() && !std::filesystem::exists(i_vpdPath, l_ec))
    {
        if (l_ec)
        {
            std::cerr
                << std::format(
                       "Filesystem call exists failed for path {}, reason: {}",
                       i_vpdPath, l_ec.message())
                << std::endl;
            return static_cast<int>(vpd::ErrorCode::FILE_SYSTEM_ERROR);
        }

        std::cerr << std::format("Given EEPROM file path {} doesn't exist.",
                                 i_vpdPath)
                  << std::endl;
        return static_cast<int>(vpd::ErrorCode::EEPROM_PATH_NOT_FOUND);
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
 * @param[in] i_fileOption - Option to pass file path.
 * @param[in] i_filePath - File path.
 * @param[in] i_chassisIdOption - Option to pass chassis Id
 * @param[in] i_chassisId - Chassis id
 *
 * @return Success if corresponding value is found against option, failure
 * otherwise.
 */
int checkOptionValuePair(const auto& i_objectOption, const auto& i_vpdPath,
                         const auto& i_recordOption, const auto& i_recordName,
                         const auto& i_keywordOption, const auto& i_keywordName,
                         const auto& i_fileOption, const auto& i_filePath,
                         const auto& i_chassisIdOption, const auto& i_chassisId)
{
    if (!i_objectOption->empty() && i_vpdPath.empty())
    {
        std::cerr << "Given path is empty." << std::endl;
        return static_cast<int>(vpd::ErrorCode::INVALID_INPUT_PARAMETER);
    }

    if (!i_recordOption->empty() &&
        (i_recordName.size() != vpd::constants::RECORD_SIZE))
    {
        std::cerr << "Record " << i_recordName << " is not supported."
                  << std::endl;
        return static_cast<int>(vpd::ErrorCode::INVALID_INPUT_PARAMETER);
    }

    if (!i_keywordOption->empty() &&
        (i_keywordName.size() != vpd::constants::KEYWORD_SIZE))
    {
        std::cerr << "Keyword " << i_keywordName << " is not supported."
                  << std::endl;
        return static_cast<int>(vpd::ErrorCode::INVALID_INPUT_PARAMETER);
    }

    if (!i_fileOption->empty() && i_filePath.empty())
    {
        std::cerr << "File path is empty." << std::endl;
        return static_cast<int>(vpd::ErrorCode::EMPTY_FILE);
    }

    if (!i_chassisIdOption->empty() && !i_chassisId)
    {
        std::cerr << "Chassis Id is empty." << std::endl;
        return static_cast<int>(vpd::ErrorCode::CHASSIS_ID_NOT_PROVIDED);
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
        "    Keyword Format:\n"
        "        From hardware to console: "
        "vpd-tool -r -H -O <EEPROM Path> -K <Keyword Name>\n"
        "        From hardware to file: "
        "vpd-tool -r -H -O <EEPROM Path> -K <Keyword Name> --file <File Path>\n"
        "    Note: If record option is not provided, it will be considered as keyword format.\n"
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
        "    Keyword Format:\n"
        "        On hardware: "
        "vpd-tool -w/-u -H -O <EEPROM Path> -K <Keyword Name> -V <Keyword Value>\n"
        "        On hardware, take keyword value from file:\n"
        "              vpd-tool -w/-u -H -O <EEPROM Path> -K <Keyword Name> --file <File Path>\n"
        "    Note: If record option is not provided, it will be considered as keyword format.\n"
        "Dump Inventory:\n"
        "   From DBus to console in JSON format: "
        "vpd-tool -i\n"
        "   From DBus to console in Table format: "
        "vpd-tool -i -t\n"
        "   Chassis based dump inventory: \n"
        "       In JSON format: vpd-tool -i -c -N <chassis_id>\n"
        "       In table format: vpd-tool -i -t -c -N <chassis_id>\n"
        "Validate EEPROM:\n"
        "   Validate given EEPROM against its redundant copy:\n"
        "   vpd-tool --validateRedundantEeprom/-e -O <EEPROM Path>\n"
        "Return Values:\n"
        "   Success:\n"
        "       Non-negative number.\n"
        "       For read and write operations return value indicates the number of bytes read/write.\n"
        "   Failure:\n"
        "       Negative values indicates the following errors.\n"
        "       -2,     Either one of the input parameter provided are invalid.\n"
        "       -3,     Record name is not provided.\n"
        "       -4,     Keyword value is not provided.\n"
        "       -5,     DBus call failed.\n"
        "       -6,     File system error.\n"
        "       -7,     File not found.\n"
        "       -8,     Standard exception occurred.\n"
        "       -9,     JSON exception.\n"
        "       -10,    EEPROM path not found.\n"
        "       -11,    Empty file.\n"
        "       -12,    Keyword name is not provided.\n"
        "       -13,    DBus returned a value of an unexpected type.\n"
        "       -14,    Requested operation is not allowed.\n"
        "       -15     Chassis id not provided.\n"
        "\n Note: vpd-tool operations are blocked while the VPD collection is in progress.\n"
#if 0
        " // Disabling these options for now, as they require additional refactoring to enable."
        "Dump Object:\n"
        "    From DBus to console: "
        "vpd-tool -o -O <DBus Object Path>\n"
        "Fix System VPD:\n"
        "    vpd-tool --fixSystemVPD\n"
        "MfgClean:\n"
        "        Flag to clean and reset specific keywords on system VPD to its default value.\n"
        "        vpd-tool --mfgClean\n"
        "        To sync BIOS attribute related keywords with BIOS Config Manager:\n"
        "        vpd-tool --mfgClean --syncBiosAttributes\n" 
        "Force Reset:\n"
        "   vpd-tool --forceReset\n"
#endif
    );
}

int main(int argc, char** argv)
{
    CLI::App l_app{"VPD Command Line Tool"};

    std::string l_vpdPath{};
    std::string l_recordName{};
    std::string l_keywordName{};
    std::string l_filePath{};
    std::string l_keywordValue{};
    std::optional<int> l_chassisId;
    std::string l_splitModeFilePath{};

    updateFooter(l_app);

    auto l_objectOption =
        l_app.add_option("--object, -O", l_vpdPath, "File path");
    auto l_recordOption =
        l_app.add_option("--record, -R", l_recordName, "Record name");
    auto l_keywordOption =
        l_app.add_option("--keyword, -K", l_keywordName, "Keyword name");

    auto l_fileOption = l_app.add_option(
        "--file", l_filePath,
        "Absolute file path,\nNote: For write operation, file should contain keyword’s value in either ascii or in hex format.");

    auto l_keywordValueOption =
        l_app.add_option("--value, -V", l_keywordValue,
                         "Keyword value in ascii/hex format."
                         " ascii ex: 01234; hex ex: 0x30313233");

    auto l_chassisIdOption =
        l_app.add_option("--chassisId, -N", l_chassisId, "Chassis Id");

    auto l_hardwareFlag =
        l_app.add_flag("--Hardware, -H", "CAUTION: Developer only option.");

    auto l_readFlag = l_app.add_flag("--readKeyword, -r", "Read keyword")
                          ->needs(l_objectOption)
                          ->needs(l_keywordOption);

    auto l_writeFlag =
        l_app
            .add_flag(
                "--writeKeyword, -w,--updateKeyword, -u",
                "Write keyword,\nNote: In case DBus path is provided, both EEPROM and DBus are updated with the given keyword's value.\nIn case EEPROM path is provided, only the given EEPROM is updated with the given keyword's value.")
            ->needs(l_objectOption)
            ->needs(l_keywordOption);

    // ToDo: Take offset value from user for hardware path.

    auto l_dumpInventoryFlag =
        l_app.add_flag("--dumpInventory, -i", "Dump all the inventory objects");

    auto l_dumpInventoryTableFlag =
        l_app.add_flag("--table, -t", "Dump inventory in table format");

    auto l_dumpChassisInventoryFlag =
        l_app.add_flag("--chassis, -c", "Dump chassis based inventory")
            ->needs(l_chassisIdOption);

    auto l_validateRedundantEepromFlag =
        l_app
            .add_flag("--validateRedundantEeprom, -e",
                      "Validate given EEPROM against its redundant EEPROM")
            ->needs(l_objectOption);

    auto l_enterSplitModeFlag =
        l_app.add_flag("--enterSplitMode", "Sets the system in split mode.");

    auto l_exitSplitModeFlag =
        l_app.add_flag("--exitSplitMode", "Exit split mode on the system.");

#if 0
    auto l_dumpObjFlag =
        l_app
            .add_flag("--dumpObject, -o",
                      "Dump specific properties of an inventory object")
            ->needs(l_objectOption);

    auto l_fixSystemVpdFlag = l_app.add_flag(
        "--fixSystemVPD",
        "Use this option to interactively fix critical system VPD keywords");

    auto l_mfgCleanFlag = l_app.add_flag(
        "--mfgClean", "Manufacturing clean on system VPD keyword");

    auto l_mfgCleanConfirmFlag = l_app.add_flag(
        "--yes", "Using this flag with --mfgClean option, assumes "
                 "yes to proceed without confirmation.");

    auto l_mfgCleanSyncBiosAttributesFlag = l_app.add_flag(
        "--syncBiosAttributes, -s",
        "Using this flag with --mfgClean option, Syncs the BIOS attribute related keywords from BIOS Config Manager service instead resetting keyword's value to default value");

    auto l_forceResetFlag = l_app.add_flag(
        "--forceReset, -f, -F",
        "Force collect for hardware. CAUTION: Developer only option.");
#endif

    CLI11_PARSE(l_app, argc, argv);

    const auto l_vpdCollectionStatus = vpd::utils::readDbusProperty(
        vpd::constants::vpdManagerService, vpd::constants::vpdManagerObjectPath,
        vpd::constants::vpdCollectionInterface,
        vpd::constants::vpdCollectionStatusProperty);

    if (!l_vpdCollectionStatus.has_value())
    {
        std::cerr << "Failed to read VPD collection status property"
                  << std::endl;
        return static_cast<int>(l_vpdCollectionStatus.error());
    }

    if (!std::get_if<std::string>(&l_vpdCollectionStatus.value()))
    {
        std::cerr << "Received invalid type from DBus." << std::endl;
        return static_cast<int>(vpd::ErrorCode::DBUS_TYPE_MISMATCH);
    }

    if (*std::get_if<std::string>(&l_vpdCollectionStatus.value()) ==
        vpd::constants::vpdCollectionInProgress)
    {
        std::cout
            << "VPD collection is currently in progress, vpd-tool operations are blocked while the collection is in progress."
            << std::endl;
        return static_cast<int>(vpd::ErrorCode::NOT_ALLOWED);
    }

    if (auto l_rc = checkOptionValuePair(
            l_objectOption, l_vpdPath, l_recordOption, l_recordName,
            l_keywordOption, l_keywordName, l_fileOption, l_filePath,
            l_chassisIdOption, l_chassisId);
        l_rc < vpd::constants::VALUE_0)
    {
        return l_rc;
    }

    if (!l_readFlag->empty())
    {
        return readKeyword(l_hardwareFlag, l_vpdPath, l_recordName,
                           l_keywordName, l_filePath);
    }

    if (!l_writeFlag->empty())
    {
        if ((l_keywordValueOption->empty() && l_fileOption->empty()) ||
            (!l_keywordValueOption->empty() && !l_fileOption->empty()))
        {
            std::cerr
                << "Please provide keyword value.\nUse --value/--file to give "
                   "keyword value. Refer --help."
                << std::endl;
            return static_cast<int>(vpd::ErrorCode::KEYWORD_VALUE_NOT_PROVIDED);
        }

        if (!l_fileOption->empty())
        {
            const auto& l_kwdValue = vpd::utils::readValueFromFile(l_filePath);

            if (!l_kwdValue)
            {
                return static_cast<int>(l_kwdValue.error());
            }

            l_keywordValue = l_kwdValue.value();

            return writeKeyword(l_hardwareFlag, l_fileOption, l_vpdPath,
                                l_recordName, l_keywordName, l_keywordValue);
        }

        return writeKeyword(l_hardwareFlag, l_keywordValueOption, l_vpdPath,
                            l_recordName, l_keywordName, l_keywordValue);
    }

    if (!l_dumpInventoryFlag->empty())
    {
        vpd::VpdTool l_vpdToolObj;
        return l_vpdToolObj.dumpInventory(
            !l_dumpChassisInventoryFlag->empty() ? l_chassisId : std::nullopt,
            !l_dumpInventoryTableFlag->empty());
    }

    if (!l_validateRedundantEepromFlag->empty())
    {
        vpd::VpdTool l_vpdToolObj;
        return l_vpdToolObj.validateRedundantEeprom(l_vpdPath);
    }

    if (!l_enterSplitModeFlag->empty())
    {
        vpd::SplitMode l_splitModeObj;
        return l_splitModeObj.setUpSplitMode(l_filePath);
    }

    if (!l_exitSplitModeFlag->empty())
    {
        vpd::SplitMode l_splitModeObj;
        return l_splitModeObj.exitSplitMode();
    }

#if 0
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
        return doMfgClean(l_mfgCleanConfirmFlag,
                          l_mfgCleanSyncBiosAttributesFlag);
    }
#endif

    std::cout << l_app.help() << std::endl;
    return vpd::constants::FAILURE;
}
