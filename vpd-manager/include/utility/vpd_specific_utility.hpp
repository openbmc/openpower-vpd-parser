#pragma once

#include "config.h"

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>
#include <utility/common_utility.hpp>
#include <utility/dbus_utility.hpp>
#include <utility/event_logger_utility.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>
#include <regex>
#include <typeindex>

namespace vpd
{
namespace vpdSpecificUtility
{
/**
 * @brief API to generate file name for bad VPD.
 *
 * For i2c eeproms - the pattern of the vpd-name will be
 * i2c-<bus-number>-<eeprom-address>.
 * For spi eeproms - the pattern of the vpd-name will be spi-<spi-number>.
 *
 * @param[in] i_vpdFilePath - file path of the vpd.
 * @param[out] o_errCode - to set error code in case of error.
 *
 * @return On success, returns generated file name, otherwise returns empty
 * string.
 */
inline std::string generateBadVPDFileName(const std::string& i_vpdFilePath,
                                          uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    std::string l_badVpdFileName{constants::badVpdDir};

    if (i_vpdFilePath.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return l_badVpdFileName;
    }

    try
    {
        if (i_vpdFilePath.find("i2c") != std::string::npos)
        {
            l_badVpdFileName += "i2c-";
            std::regex l_i2cPattern("(at24/)([0-9]+-[0-9]+)\\/");
            std::smatch l_match;
            if (std::regex_search(i_vpdFilePath, l_match, l_i2cPattern))
            {
                l_badVpdFileName += l_match.str(2);
            }
        }
        else if (i_vpdFilePath.find("spi") != std::string::npos)
        {
            std::regex l_spiPattern("((spi)[0-9]+)(.0)");
            std::smatch l_match;
            if (std::regex_search(i_vpdFilePath, l_match, l_spiPattern))
            {
                l_badVpdFileName += l_match.str(1);
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        l_badVpdFileName.clear();
        o_errCode = error_code::STANDARD_EXCEPTION;
    }
    return l_badVpdFileName;
}

/**
 * @brief API which dumps the broken/bad vpd in a directory.
 * When the vpd is bad, this API places  the bad vpd file inside
 * "/var/lib/vpd/dumps" in BMC, in order to collect bad VPD data as a part of
 * user initiated BMC dump.
 *
 *
 * @param[in] i_vpdFilePath - vpd file path
 * @param[in] i_vpdVector - vpd vector
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return On success returns 0, otherwise returns -1.
 */
inline int dumpBadVpd(const std::string& i_vpdFilePath,
                      const types::BinaryVector& i_vpdVector,
                      uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    if (i_vpdFilePath.empty() || i_vpdVector.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return constants::FAILURE;
    }

    int l_rc{constants::FAILURE};
    try
    {
        std::filesystem::create_directory(constants::badVpdDir);
        auto l_badVpdPath = generateBadVPDFileName(i_vpdFilePath, o_errCode);

        if (l_badVpdPath.empty())
        {
            if (o_errCode)
            {
                logging::logMessage("Failed to create bad VPD file name : " +
                                    commonUtility::getErrCodeMsg(o_errCode));
            }

            return constants::FAILURE;
        }

        if (std::filesystem::exists(l_badVpdPath))
        {
            std::error_code l_ec;
            std::filesystem::remove(l_badVpdPath, l_ec);
            if (l_ec) // error code
            {
                o_errCode = error_code::FILE_SYSTEM_ERROR;
                return constants::FAILURE;
            }
        }

        std::ofstream l_badVpdFileStream(l_badVpdPath, std::ofstream::binary);
        if (!l_badVpdFileStream.is_open())
        {
            o_errCode = error_code::FILE_ACCESS_ERROR;
            return constants::FAILURE;
        }

        l_badVpdFileStream.write(
            reinterpret_cast<const char*>(i_vpdVector.data()),
            i_vpdVector.size());

        l_rc = constants::SUCCESS;
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

/**
 * @brief An API to read value of a keyword.
 *
 *
 * @param[in] i_kwdValueMap - A map having Kwd value pair.
 * @param[in] i_kwd - keyword name.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return On success returns value of the keyword read from map, otherwise
 * returns empty string.
 */
inline std::string getKwVal(const types::IPZKwdValueMap& i_kwdValueMap,
                            const std::string& i_kwd,
                            uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    std::string l_kwdValue;
    if (i_kwd.empty() || i_kwdValueMap.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return l_kwdValue;
    }

    auto l_itrToKwd = i_kwdValueMap.find(i_kwd);
    if (l_itrToKwd != i_kwdValueMap.end())
    {
        l_kwdValue = l_itrToKwd->second;
    }
    else
    {
        o_errCode = error_code::KEYWORD_NOT_FOUND;
    }

    return l_kwdValue;
}

/**
 * @brief An API to process encoding of a keyword.
 *
 * @param[in] i_keyword - Keyword to be processed.
 * @param[in] i_encoding - Type of encoding.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return Value after being processed for encoded type.
 */
inline std::string encodeKeyword(const std::string& i_keyword,
                                 const std::string& i_encoding,
                                 uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    if (i_keyword.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }

    // Default value is keyword value
    std::string l_result(i_keyword.begin(), i_keyword.end());

    if (i_encoding.empty())
    {
        return l_result;
    }

    try
    {
        if (i_encoding == "MAC")
        {
            l_result.clear();
            size_t l_firstByte = i_keyword[0];

            auto l_hexValue = commonUtility::toHex(l_firstByte >> 4);

            if (!l_hexValue)
            {
                o_errCode = error_code::OUT_OF_BOUND_EXCEPTION;
                return std::string{};
            }

            l_result += l_hexValue;

            l_hexValue = commonUtility::toHex(l_firstByte & 0x0f);

            if (!l_hexValue)
            {
                o_errCode = error_code::OUT_OF_BOUND_EXCEPTION;
                return std::string{};
            }

            l_result += l_hexValue;

            for (size_t i = 1; i < i_keyword.size(); ++i)
            {
                l_result += ":";

                l_hexValue = commonUtility::toHex(i_keyword[i] >> 4);

                if (!l_hexValue)
                {
                    o_errCode = error_code::OUT_OF_BOUND_EXCEPTION;
                    return std::string{};
                }

                l_result += l_hexValue;

                l_hexValue = commonUtility::toHex(i_keyword[i] & 0x0f);

                if (!l_hexValue)
                {
                    o_errCode = error_code::OUT_OF_BOUND_EXCEPTION;
                    return std::string{};
                }

                l_result += l_hexValue;
            }
        }
        else if (i_encoding == "DATE")
        {
            // Date, represent as
            // <year>-<month>-<day> <hour>:<min>
            l_result.clear();
            static constexpr uint8_t skipPrefix = 3;

            auto strItr = i_keyword.begin();
            advance(strItr, skipPrefix);
            for_each(strItr, i_keyword.end(),
                     [&l_result](size_t c) { l_result += c; });

            l_result.insert(constants::BD_YEAR_END, 1, '-');
            l_result.insert(constants::BD_MONTH_END, 1, '-');
            l_result.insert(constants::BD_DAY_END, 1, ' ');
            l_result.insert(constants::BD_HOUR_END, 1, ':');
        }
    }
    catch (const std::exception& l_ex)
    {
        l_result.clear();
        o_errCode = error_code::STANDARD_EXCEPTION;
    }

    return l_result;
}

/**
 * @brief Helper function to insert or merge in map.
 *
 * This method checks in an interface if the given interface exists. If the
 * interface key already exists, property map is inserted corresponding to it.
 * If the key doesn't exist then given interface and property map pair is newly
 * created. If the property present in propertymap already exist in the
 * InterfaceMap, then the new property value is ignored.
 *
 * @param[in,out] io_map - Interface map.
 * @param[in] i_interface - Interface to be processed.
 * @param[in] i_propertyMap - new property map that needs to be emplaced.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return On success returns 0, otherwise returns -1.
 */
inline int insertOrMerge(types::InterfaceMap& io_map,
                         const std::string& i_interface,
                         types::PropertyMap&& i_propertyMap,
                         uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    int l_rc{constants::FAILURE};

    try
    {
        if (io_map.find(i_interface) != io_map.end())
        {
            auto& l_prop = io_map.at(i_interface);
            std::for_each(i_propertyMap.begin(), i_propertyMap.end(),
                          [&l_prop](auto l_keyValue) {
                              l_prop[l_keyValue.first] = l_keyValue.second;
                          });
        }
        else
        {
            io_map.emplace(i_interface, i_propertyMap);
        }

        l_rc = constants::SUCCESS;
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

/**
 * @brief An API to get VPD in a vector.
 *
 * The vector is required by the respective parser to fill the VPD map.
 * Note: API throws exception in case of failure. Caller needs to handle.
 *
 * @param[in] vpdFilePath - EEPROM path of the FRU.
 * @param[out] vpdVector - VPD in vector form.
 * @param[in] vpdStartOffset - Offset of VPD data in EEPROM.
 * @param[out] o_errCode - To set error code in case of error.
 */
inline void getVpdDataInVector(const std::string& vpdFilePath,
                               types::BinaryVector& vpdVector,
                               size_t& vpdStartOffset, uint16_t& o_errCode)
{
    o_errCode = 0;
    if (vpdFilePath.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return;
    }

    try
    {
        std::fstream vpdFileStream;
        vpdFileStream.exceptions(
            std::ifstream::badbit | std::ifstream::failbit);
        vpdFileStream.open(vpdFilePath, std::ios::in | std::ios::binary);
        auto vpdSizeToRead = std::min(std::filesystem::file_size(vpdFilePath),
                                      static_cast<uintmax_t>(65504));
        vpdVector.resize(vpdSizeToRead);

        vpdFileStream.seekg(vpdStartOffset, std::ios_base::beg);
        vpdFileStream.read(reinterpret_cast<char*>(&vpdVector[0]),
                           vpdSizeToRead);

        vpdVector.resize(vpdFileStream.gcount());
        vpdFileStream.clear(std::ios_base::eofbit);
    }
    catch (const std::ifstream::failure& fail)
    {
        o_errCode = error_code::FILE_SYSTEM_ERROR;
        return;
    }
}

/**
 * @brief An API to get D-bus representation of given VPD keyword.
 *
 * @param[in] i_keywordName - VPD keyword name.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return D-bus representation of given keyword.
 */
inline std::string getDbusPropNameForGivenKw(const std::string& i_keywordName,
                                             uint16_t& o_errCode)
{
    o_errCode = 0;
    if (i_keywordName.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }
    // Check for "#" prefixed VPD keyword.
    if ((i_keywordName.size() == vpd::constants::TWO_BYTES) &&
        (i_keywordName.at(0) == constants::POUND_KW))
    {
        // D-bus doesn't support "#". Replace "#" with "PD_" for those "#"
        // prefixed keywords.
        return (std::string(constants::POUND_KW_PREFIX) +
                i_keywordName.substr(1));
    }

    // Return the keyword name back, if D-bus representation is same as the VPD
    // keyword name.
    return i_keywordName;
}

/**
 * @brief API to find CCIN in parsed VPD map.
 *
 * Few FRUs need some special handling. To identify those FRUs CCIN are used.
 * The API will check from parsed VPD map if the FRU is the one with desired
 * CCIN.
 *
 * @param[in] i_JsonObject - Any JSON which contains CCIN tag to match.
 * @param[in] i_parsedVpdMap - Parsed VPD map.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return True if found, false otherwise.
 */
inline bool findCcinInVpd(const nlohmann::json& i_JsonObject,
                          const types::VPDMapVariant& i_parsedVpdMap,
                          uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    bool l_rc{false};
    try
    {
        if (i_JsonObject.empty() ||
            std::holds_alternative<std::monostate>(i_parsedVpdMap))
        {
            o_errCode = error_code::INVALID_INPUT_PARAMETER;
            return l_rc;
        }

        if (auto l_ipzVPDMap = std::get_if<types::IPZVpdMap>(&i_parsedVpdMap))
        {
            auto l_itrToRec = (*l_ipzVPDMap).find("VINI");
            if (l_itrToRec == (*l_ipzVPDMap).end())
            {
                o_errCode = error_code::RECORD_NOT_FOUND;
                return l_rc;
            }

            std::string l_ccinFromVpd{vpdSpecificUtility::getKwVal(
                l_itrToRec->second, "CC", o_errCode)};
            if (l_ccinFromVpd.empty())
            {
                o_errCode = error_code::KEYWORD_NOT_FOUND;
                return l_rc;
            }

            transform(l_ccinFromVpd.begin(), l_ccinFromVpd.end(),
                      l_ccinFromVpd.begin(), ::toupper);

            for (std::string l_ccinValue : i_JsonObject["ccin"])
            {
                transform(l_ccinValue.begin(), l_ccinValue.end(),
                          l_ccinValue.begin(), ::toupper);

                if (l_ccinValue.compare(l_ccinFromVpd) ==
                    constants::STR_CMP_SUCCESS)
                {
                    // CCIN found
                    l_rc = true;
                }
            }

            if (!l_rc)
            {
                logging::logMessage("No match found for CCIN");
            }
        }
        else
        {
            o_errCode = error_code::UNSUPPORTED_VPD_TYPE;
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }
    return l_rc;
}

/**
 * @brief API to reset data of a FRU populated under PIM.
 *
 * This API resets the data for particular interfaces of a FRU under PIM.
 *
 * @param[in] i_objectPath - DBus object path of the FRU.
 * @param[in] io_interfaceMap - Interface and its properties map.
 * @param[in] i_clearPresence - Indicates whether to clear present property or
 * not.
 * @param[out] o_errCode - To set error code in case of error.
 */
inline void resetDataUnderPIM(const std::string& i_objectPath,
                              types::InterfaceMap& io_interfaceMap,
                              bool i_clearPresence, uint16_t& o_errCode)
{
    o_errCode = 0;
    if (i_objectPath.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return;
    }

    try
    {
        std::vector<std::string> l_interfaces;
        const types::MapperGetObject& l_getObjectMap =
            dbusUtility::getObjectMap(i_objectPath, l_interfaces);

        static const std::vector<std::string> l_vpdRelatedInterfaces{
            constants::operationalStatusInf, constants::inventoryItemInf,
            constants::assetInf, constants::vpdCollectionInterface};

        for (const auto& [l_service, l_interfaceList] : l_getObjectMap)
        {
            if (l_service.compare(constants::pimServiceName) !=
                constants::STR_CMP_SUCCESS)
            {
                continue;
            }

            for (const auto& l_interface : l_interfaceList)
            {
                if ((l_interface.find(constants::ipzVpdInf) !=
                         std::string::npos &&
                     l_interface != constants::locationCodeInf) ||
                    ((std::find(l_vpdRelatedInterfaces.begin(),
                                l_vpdRelatedInterfaces.end(), l_interface)) !=
                     l_vpdRelatedInterfaces.end()))
                {
                    const types::PropertyMap& l_propertyValueMap =
                        dbusUtility::getPropertyMap(l_service, i_objectPath,
                                                    l_interface);

                    types::PropertyMap l_propertyMap;

                    for (const auto& l_aProperty : l_propertyValueMap)
                    {
                        const std::string& l_propertyName = l_aProperty.first;
                        const auto& l_propertyValue = l_aProperty.second;

                        if (std::holds_alternative<types::BinaryVector>(
                                l_propertyValue))
                        {
                            l_propertyMap.emplace(l_propertyName,
                                                  types::BinaryVector{});
                        }
                        else if (std::holds_alternative<std::string>(
                                     l_propertyValue))
                        {
                            if (l_propertyName.compare("Status") ==
                                constants::STR_CMP_SUCCESS)
                            {
                                l_propertyMap.emplace(
                                    l_propertyName,
                                    constants::vpdCollectionNotStarted);
                                l_propertyMap.emplace("StartTime", 0);
                                l_propertyMap.emplace("CompletedTime", 0);
                            }
                            else if (l_propertyName.compare("PrettyName") ==
                                     constants::STR_CMP_SUCCESS)
                            {
                                // The FRU name is constant and independent of
                                // its presence state. So, it should not get
                                // reset.
                                continue;
                            }
                            else
                            {
                                l_propertyMap.emplace(l_propertyName,
                                                      std::string{});
                            }
                        }
                        else if (std::holds_alternative<bool>(l_propertyValue))
                        {
                            if (l_propertyName.compare("Present") ==
                                constants::STR_CMP_SUCCESS)
                            {
                                if (i_clearPresence)
                                {
                                    l_propertyMap.emplace(l_propertyName,
                                                          false);
                                }
                            }
                            else if (l_propertyName.compare("Functional") ==
                                     constants::STR_CMP_SUCCESS)
                            {
                                // Since FRU is not present functional property
                                // is considered as true.
                                l_propertyMap.emplace(l_propertyName, true);
                            }
                        }
                    }
                    io_interfaceMap.emplace(l_interface,
                                            std::move(l_propertyMap));
                }
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }
}

/**
 * @brief API to detect pass1 planar type.
 *
 * Based on HW version and IM keyword, This API detects is it is a pass1 planar
 * or not.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return True if pass 1 planar, false otherwise.
 */
inline bool isPass1Planar(uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    bool l_rc{false};
    const auto l_hwVar = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::viniInf, constants::kwdHW);

    auto l_hwVer = std::get_if<types::BinaryVector>(&l_hwVar);
    if (l_hwVer == nullptr)
    {
        o_errCode = error_code::INVALID_VALUE_READ_FROM_DBUS;
        return l_rc;
    }

    const auto l_imVar = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsbpInf, constants::kwdIM);

    auto l_imValue = std::get_if<types::BinaryVector>(&l_imVar);
    if (l_imValue == nullptr)
    {
        o_errCode = error_code::INVALID_VALUE_READ_FROM_DBUS;
        return l_rc;
    }

    if (l_hwVer->size() != constants::VALUE_2)
    {
        o_errCode = error_code::INVALID_KEYWORD_LENGTH;
        return l_rc;
    }

    if (l_imValue->size() != constants::VALUE_4)
    {
        o_errCode = error_code::INVALID_KEYWORD_LENGTH;
        return l_rc;
    }

    const types::BinaryVector l_everest{80, 00, 48, 00};
    const types::BinaryVector l_fuji{96, 00, 32, 00};

    if (((*l_imValue) == l_everest) || ((*l_imValue) == l_fuji))
    {
        if ((*l_hwVer).at(1) < constants::VALUE_21)
        {
            l_rc = true;
        }
    }
    else if ((*l_hwVer).at(1) < constants::VALUE_2)
    {
        l_rc = true;
    }

    return l_rc;
}

/**
 * @brief API to sync keyword update to inherited FRUs.
 *
 * For a given keyword update on a EEPROM path, this API syncs the keyword
 * update to all inherited FRUs' respective interface, property on PIM.
 *
 * @param[in] i_fruPath - EEPROM path of FRU.
 * @param[in] i_paramsToWriteData - Input details.
 * @param[in] i_sysCfgJsonObj - System config JSON.
 * @param[out] o_errCode - To set error code in case of error.
 *
 */
inline void updateKwdOnInheritedFrus(
    const std::string& i_fruPath,
    const types::WriteVpdParams& i_paramsToWriteData,
    const nlohmann::json& i_sysCfgJsonObj, uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    try
    {
        if (i_fruPath.empty() || i_sysCfgJsonObj.empty())
        {
            o_errCode = error_code::INVALID_INPUT_PARAMETER;
            return;
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            o_errCode = error_code::INVALID_JSON;
            return;
        }

        if (!i_sysCfgJsonObj["frus"].contains(i_fruPath))
        {
            o_errCode = error_code::FRU_PATH_NOT_FOUND;
            return;
        }

        const types::IpzData* l_ipzData =
            std::get_if<types::IpzData>(&i_paramsToWriteData);

        if (!l_ipzData)
        {
            o_errCode = error_code::UNSUPPORTED_VPD_TYPE;
            return;
        }
        //  iterate through all inventory paths for given EEPROM path,
        //  except the base FRU.
        //  if for an inventory path, "inherit" tag is true,
        //  update the inventory path's com.ibm.ipzvpd.<record>,keyword
        //  property

        types::ObjectMap l_objectInterfaceMap;

        auto l_populateInterfaceMap =
            [&l_objectInterfaceMap,
             &l_ipzData = std::as_const(l_ipzData)](const auto& l_Fru) {
                // update inherited FRUs only
                if (l_Fru.value("inherit", true))
                {
                    l_objectInterfaceMap.emplace(
                        sdbusplus::object_path{l_Fru["inventoryPath"]},
                        types::InterfaceMap{
                            {std::string{constants::ipzVpdInf +
                                         std::get<0>(*l_ipzData)},
                             types::PropertyMap{{std::get<1>(*l_ipzData),
                                                 std::get<2>(*l_ipzData)}}}});
                }
            };

        // iterate through all FRUs except the base FRU
        std::for_each(
            i_sysCfgJsonObj["frus"][i_fruPath].begin() + constants::VALUE_1,
            i_sysCfgJsonObj["frus"][i_fruPath].end(), l_populateInterfaceMap);

        if (!l_objectInterfaceMap.empty())
        {
            // Call method to update the dbus
            if (!dbusUtility::publishVpdOnDBus(move(l_objectInterfaceMap)))
            {
                o_errCode = error_code::DBUS_FAILURE;
                return;
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
        return;
    }
}

/**
 * @brief API to get common interface(s) properties corresponding to given
 * record and keyword.
 *
 * For a given record and keyword, this API finds the corresponding common
 * interfaces(s) properties from the system config JSON and populates an
 * interface map with the respective properties and values.
 *
 * @param[in] i_paramsToWriteData - Input details.
 * @param[in] i_commonInterfaceJson - Common interface JSON object.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return Returns a map of common interface(s) and properties corresponding to
 * the record and keyword. An empty map is returned if no such common
 * interface(s) and properties are found.
 */
inline types::InterfaceMap getCommonInterfaceProperties(
    const types::WriteVpdParams& i_paramsToWriteData,
    const nlohmann::json& i_commonInterfaceJson, uint16_t& o_errCode) noexcept
{
    types::InterfaceMap l_interfaceMap;
    o_errCode = 0;
    try
    {
        if (i_commonInterfaceJson.empty())
        {
            o_errCode = error_code::INVALID_INPUT_PARAMETER;
            return l_interfaceMap;
        }

        const types::IpzData* l_ipzData =
            std::get_if<types::IpzData>(&i_paramsToWriteData);

        if (!l_ipzData)
        {
            o_errCode = error_code::UNSUPPORTED_VPD_TYPE;
            return l_interfaceMap;
        }

        auto l_populateInterfaceMap = [&l_ipzData = std::as_const(l_ipzData),
                                       &l_interfaceMap, &o_errCode](
                                          const auto& l_interfacesPropPair) {
            if (l_interfacesPropPair.value().empty())
            {
                return;
            }

            // find matching property value pair
            const auto l_matchPropValuePairIt = std::find_if(
                l_interfacesPropPair.value().items().begin(),
                l_interfacesPropPair.value().items().end(),
                [&l_ipzData](const auto& l_propValuePair) {
                    return (l_propValuePair.value().value("recordName", "") ==
                                std::get<0>(*l_ipzData) &&
                            l_propValuePair.value().value("keywordName", "") ==
                                std::get<1>(*l_ipzData));
                });

            if (l_matchPropValuePairIt !=
                l_interfacesPropPair.value().items().end())
            {
                std::string l_kwd = std::string(std::get<2>(*l_ipzData).begin(),
                                                std::get<2>(*l_ipzData).end());

                std::string l_encodedValue = vpdSpecificUtility::encodeKeyword(
                    l_kwd, l_matchPropValuePairIt.value().value("encoding", ""),
                    o_errCode);

                if (l_encodedValue.empty() && o_errCode)
                {
                    logging::logMessage(
                        "Failed to get encoded value for keyword : " + l_kwd +
                        ", error : " + commonUtility::getErrCodeMsg(o_errCode));
                }

                // add property map to interface map
                l_interfaceMap.emplace(
                    l_interfacesPropPair.key(),
                    types::PropertyMap{
                        {l_matchPropValuePairIt.key(), l_encodedValue}});
            }
        };

        if (!i_commonInterfaceJson.empty())
        {
            // iterate through all common interfaces and populate interface map
            std::for_each(i_commonInterfaceJson.items().begin(),
                          i_commonInterfaceJson.items().end(),
                          l_populateInterfaceMap);
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }
    return l_interfaceMap;
}

/**
 * @brief API to update common interface(s) properties when keyword is updated.
 *
 * For a given keyword update on a EEPROM path, this API syncs the keyword
 * update to respective common interface(s) properties of the base FRU and all
 * inherited FRUs.
 *
 * @param[in] i_fruPath - EEPROM path of FRU.
 * @param[in] i_paramsToWriteData - Input details.
 * @param[in] i_sysCfgJsonObj - System config JSON.
 * @param[out] o_errCode - To set error code in case of error.
 */
inline void updateCiPropertyOfInheritedFrus(
    const std::string& i_fruPath,
    const types::WriteVpdParams& i_paramsToWriteData,
    const nlohmann::json& i_sysCfgJsonObj, uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    try
    {
        if (i_fruPath.empty() || i_sysCfgJsonObj.empty())
        {
            o_errCode = error_code::INVALID_INPUT_PARAMETER;
            return;
        }

        if (!i_sysCfgJsonObj.contains("commonInterfaces"))
        {
            // no common interfaces in JSON, nothing to do
            return;
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            o_errCode = error_code::INVALID_JSON;
            return;
        }

        if (!i_sysCfgJsonObj["frus"].contains(i_fruPath))
        {
            o_errCode = error_code::FRU_PATH_NOT_FOUND;
            return;
        }

        if (!std::get_if<types::IpzData>(&i_paramsToWriteData))
        {
            o_errCode = error_code::UNSUPPORTED_VPD_TYPE;
            return;
        }

        //  iterate through all inventory paths for given EEPROM path,
        //  if for an inventory path, "inherit" tag is true,
        //  update the inventory path's com.ibm.ipzvpd.<record>,keyword
        //  property

        types::ObjectMap l_objectInterfaceMap;

        const types::InterfaceMap l_interfaceMap = getCommonInterfaceProperties(
            i_paramsToWriteData, i_sysCfgJsonObj["commonInterfaces"],
            o_errCode);

        if (l_interfaceMap.empty())
        {
            if (o_errCode)
            {
                logging::logMessage(
                    "Failed to get common interface property list, error : " +
                    commonUtility::getErrCodeMsg(o_errCode));
            }
            // nothing to do
            return;
        }

        auto l_populateObjectInterfaceMap =
            [&l_objectInterfaceMap, &l_interfaceMap = std::as_const(
                                        l_interfaceMap)](const auto& l_Fru) {
                if (l_Fru.value("inherit", true) &&
                    l_Fru.contains("inventoryPath"))
                {
                    l_objectInterfaceMap.emplace(
                        sdbusplus::object_path{l_Fru["inventoryPath"]},
                        l_interfaceMap);
                }
            };

        std::for_each(i_sysCfgJsonObj["frus"][i_fruPath].begin(),
                      i_sysCfgJsonObj["frus"][i_fruPath].end(),
                      l_populateObjectInterfaceMap);

        if (!l_objectInterfaceMap.empty())
        {
            // Call method to update the dbus
            if (!dbusUtility::publishVpdOnDBus(move(l_objectInterfaceMap)))
            {
                o_errCode = error_code::DBUS_FAILURE;
                return;
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }
}

/**
 * @brief API to convert write VPD parameters to a string.
 *
 * @param[in] i_paramsToWriteData - write VPD parameters.
 * @param[out] o_errCode - To set error code in case of error.
 *
 * @return On success returns string representation of write VPD parameters,
 * otherwise returns an empty string.
 */
inline const std::string convertWriteVpdParamsToString(
    const types::WriteVpdParams& i_paramsToWriteData,
    uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    try
    {
        if (const types::IpzData* l_ipzDataPtr =
                std::get_if<types::IpzData>(&i_paramsToWriteData))
        {
            return std::string{
                "Record: " + std::get<0>(*l_ipzDataPtr) +
                " Keyword: " + std::get<1>(*l_ipzDataPtr) + " Value: " +
                commonUtility::convertByteVectorToHex(
                    std::get<2>(*l_ipzDataPtr))};
        }
        else if (const types::KwData* l_kwDataPtr =
                     std::get_if<types::KwData>(&i_paramsToWriteData))
        {
            return std::string{
                "Keyword: " + std::get<0>(*l_kwDataPtr) + " Value: " +
                commonUtility::convertByteVectorToHex(
                    std::get<1>(*l_kwDataPtr))};
        }
        else
        {
            o_errCode = error_code::UNSUPPORTED_VPD_TYPE;
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;
    }
    return std::string{};
}

/**
 * @brief An API to read IM value from VPD.
 *
 * @param[in] i_parsedVpd - Parsed VPD.
 * @param[out] o_errCode - To set error code in case of error.
 */
inline std::string getIMValue(const types::IPZVpdMap& i_parsedVpd,
                              uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    std::ostringstream l_imData;
    try
    {
        if (i_parsedVpd.empty())
        {
            o_errCode = error_code::INVALID_INPUT_PARAMETER;
            return {};
        }

        const auto& l_itrToVSBP = i_parsedVpd.find("VSBP");
        if (l_itrToVSBP == i_parsedVpd.end())
        {
            o_errCode = error_code::RECORD_NOT_FOUND;
            return {};
        }

        const auto& l_itrToIM = (l_itrToVSBP->second).find("IM");
        if (l_itrToIM == (l_itrToVSBP->second).end())
        {
            o_errCode = error_code::KEYWORD_NOT_FOUND;
            return {};
        }

        types::BinaryVector l_imVal;
        std::copy(l_itrToIM->second.begin(), l_itrToIM->second.end(),
                  back_inserter(l_imVal));

        for (auto& l_aByte : l_imVal)
        {
            l_imData << std::setw(2) << std::setfill('0') << std::hex
                     << static_cast<int>(l_aByte);
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to get IM value with exception:" +
                            std::string(l_ex.what()));
        o_errCode = error_code::STANDARD_EXCEPTION;
    }

    return l_imData.str();
}

/**
 * @brief An API to read HW version from VPD.
 *
 * @param[in] i_parsedVpd - Parsed VPD.
 * @param[out] o_errCode - To set error code in case of error.
 */
inline std::string getHWVersion(const types::IPZVpdMap& i_parsedVpd,
                                uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    std::ostringstream l_hwString;
    try
    {
        if (i_parsedVpd.empty())
        {
            o_errCode = error_code::INVALID_INPUT_PARAMETER;
            return {};
        }

        const auto& l_itrToVINI = i_parsedVpd.find("VINI");
        if (l_itrToVINI == i_parsedVpd.end())
        {
            o_errCode = error_code::RECORD_NOT_FOUND;
            return {};
        }

        const auto& l_itrToHW = (l_itrToVINI->second).find("HW");
        if (l_itrToHW == (l_itrToVINI->second).end())
        {
            o_errCode = error_code::KEYWORD_NOT_FOUND;
            return {};
        }

        types::BinaryVector l_hwVal;
        std::copy(l_itrToHW->second.begin(), l_itrToHW->second.end(),
                  back_inserter(l_hwVal));

        // The planar pass only comes from the LSB of the HW keyword,
        // where as the MSB is used for other purposes such as signifying clock
        // termination.
        l_hwVal[0] = 0x00;

        for (auto& l_aByte : l_hwVal)
        {
            l_hwString << std::setw(2) << std::setfill('0') << std::hex
                       << static_cast<int>(l_aByte);
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to get HW version with exception:" +
                            std::string(l_ex.what()));
        o_errCode = error_code::STANDARD_EXCEPTION;
    }

    return l_hwString.str();
}

/**
 * @brief An API to set VPD collection status for a fru.
 *
 * This API updates the CollectionStatus property of the given FRU with the
 * given value.
 *
 * @param[in] i_vpdPath - Fru path (EEPROM or Inventory path)
 * @param[in] i_value - State to set.
 * @param[in] i_sysCfgJsonObj - System config json object.
 * @param[out] o_errCode - To set error code in case of error.
 */
inline void setCollectionStatusProperty(
    const std::string& i_vpdPath, const types::VpdCollectionStatus& i_value,
    const nlohmann::json& i_sysCfgJsonObj, uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    if (i_vpdPath.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return;
    }

    if (i_sysCfgJsonObj.empty() || !i_sysCfgJsonObj.contains("frus"))
    {
        o_errCode = error_code::INVALID_JSON;
        return;
    }

    types::PropertyMap l_timeStampMap;
    if (i_value == types::VpdCollectionStatus::Completed ||
        i_value == types::VpdCollectionStatus::Failed)
    {
        l_timeStampMap.emplace(
            "CompletedTime",
            types::DbusVariantType{commonUtility::getCurrentTimeSinceEpoch()});
    }
    else if (i_value == types::VpdCollectionStatus::InProgress)
    {
        l_timeStampMap.emplace(
            "StartTime",
            types::DbusVariantType{commonUtility::getCurrentTimeSinceEpoch()});
    }
    else if (i_value == types::VpdCollectionStatus::NotStarted)
    {
        l_timeStampMap.emplace("StartTime", 0);
        l_timeStampMap.emplace("CompletedTime", 0);
    }

    types::ObjectMap l_objectInterfaceMap;

    const auto& l_eepromPath =
        jsonUtility::getFruPathFromJson(i_sysCfgJsonObj, i_vpdPath, o_errCode);

    if (l_eepromPath.empty() || o_errCode)
    {
        return;
    }

    for (const auto& l_Fru : i_sysCfgJsonObj["frus"][l_eepromPath])
    {
        sdbusplus::object_path l_fruObjectPath(l_Fru["inventoryPath"]);

        types::PropertyMap l_propertyValueMap;
        l_propertyValueMap.emplace(
            "Status",
            types::CommonProgress::convertOperationStatusToString(i_value));
        l_propertyValueMap.insert(l_timeStampMap.begin(), l_timeStampMap.end());

        types::InterfaceMap l_interfaces;
        vpdSpecificUtility::insertOrMerge(l_interfaces,
                                          types::CommonProgress::interface,
                                          move(l_propertyValueMap), o_errCode);

        if (o_errCode)
        {
            Logger::getLoggerInstance()->logMessage(
                "Failed to insert value into map, error : " +
                commonUtility::getErrCodeMsg(o_errCode));
            return;
        }

        l_objectInterfaceMap.emplace(std::move(l_fruObjectPath),
                                     std::move(l_interfaces));
    }

    // Call dbus method to update on dbus
    if (!dbusUtility::publishVpdOnDBus(move(l_objectInterfaceMap)))
    {
        o_errCode = error_code::DBUS_FAILURE;
        return;
    }
}

/**
 * @brief API to reset data of a FRU and its sub-FRU populated under PIM.
 *
 * The API resets the data for specific interfaces of a FRU and its sub-FRUs
 * under PIM.
 *
 * Note: i_vpdPath should be either the base inventory path or the EEPROM path.
 *
 * @param[in] i_vpdPath - EEPROM/root inventory path of the FRU.
 * @param[in] i_sysCfgJsonObj - system config JSON.
 * @param[out] o_errCode - To set error code in case of error.
 */
inline void resetObjTreeVpd(const std::string& i_vpdPath,
                            const nlohmann::json& i_sysCfgJsonObj,
                            uint16_t& o_errCode) noexcept
{
    o_errCode = 0;
    if (i_vpdPath.empty() || i_sysCfgJsonObj.empty())
    {
        o_errCode = error_code::INVALID_INPUT_PARAMETER;
        return;
    }

    try
    {
        const std::string& l_fruPath = jsonUtility::getFruPathFromJson(
            i_sysCfgJsonObj, i_vpdPath, o_errCode);

        if (o_errCode)
        {
            return;
        }

        types::ObjectMap l_objectMap;

        const auto& l_fruItems = i_sysCfgJsonObj["frus"][l_fruPath];

        for (const auto& l_inventoryItem : l_fruItems)
        {
            const std::string& l_objectPath =
                l_inventoryItem.value("inventoryPath", "");

            if (l_inventoryItem.value("synthesized", false))
            {
                continue;
            }

            types::InterfaceMap l_interfaceMap;
            resetDataUnderPIM(l_objectPath, l_interfaceMap,
                              l_inventoryItem.value("handlePresence", true),
                              o_errCode);

            if (o_errCode)
            {
                logging::logMessage(
                    "Failed to get data to clear on DBus for path [" +
                    l_objectPath +
                    "], error : " + commonUtility::getErrCodeMsg(o_errCode));

                continue;
            }

            l_objectMap.emplace(l_objectPath, l_interfaceMap);
        }

        if (!dbusUtility::publishVpdOnDBus(std::move(l_objectMap)))
        {
            o_errCode = error_code::DBUS_FAILURE;
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to reset FRU data on DBus for FRU [" + i_vpdPath +
            "], error : " + std::string(l_ex.what()));

        o_errCode = error_code::STANDARD_EXCEPTION;
    }
}

/**
 * @brief Converts a record-to-keywords map into a human-readable string.
 *
 * This API formats the contents of the provided RecordKeywordsMap
 * into a printable string representation. Each record and its associated
 * keywords are appended in a structured format.
 *
 * @param[in] i_recordKeywordMap  Map of record names to their keyword list.
 *
 * @return A formatted string containing the map contents, or an empty
 *         string if the input map is empty.
 *
 * @note The output string is generated in the following format:
 *       [{record1:[kw1, kw2]}, {record2:[kw3, kw4]}, ...]
 *       Example:  [{VSYS:[BR, J0]}]
 */
inline std::string getInStringFormat(
    const types::RecordKeywordsMap& i_recordKeywordMap) noexcept
{
    if (i_recordKeywordMap.empty())
    {
        return std::string{};
    }

    std::ostringstream l_message;
    l_message << "[";
    bool l_firstHandled = false;

    for (const auto& l_recordKws : i_recordKeywordMap)
    {
        if (l_firstHandled)
        {
            l_message << ", ";
        }

        const auto l_keywords =
            l_recordKws.second | std::views::join_with(std::string(", "));

        l_message << "{" + l_recordKws.first + ":[" +
                         std::string(l_keywords.begin(), l_keywords.end()) +
                         "]}";
        l_firstHandled = true;
    }

    l_message << "]. ";
    return l_message.str();
}

/**
 * @brief Extract chassis ID from given inventory object path.
 *
 * @param[in] i_inventoryObjPath - Inventory object path.
 * @param[out] o_errCode - Error code, 0 on success.
 *
 * @return Chassis ID on successful extraction, empty string otherwise.
 */
inline std::string getChassisId(const std::string& i_inventoryObjPath,
                                uint16_t& o_errCode) noexcept
{
    try
    {
        auto l_startPos = i_inventoryObjPath.find("/chassis");
        if (std::string::npos == l_startPos)
        {
            return std::string{};
        }

        ++l_startPos;
        const auto l_endPos = i_inventoryObjPath.find('/', l_startPos);
        const auto l_chassisId =
            i_inventoryObjPath.substr(l_startPos, l_endPos - l_startPos);

        return l_chassisId;
    }
    catch (const std::exception& l_ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to extract chassis ID from given path {}, error: {}",
            i_inventoryObjPath, l_ex.what()));
        o_errCode = error_code::STANDARD_EXCEPTION;
        return std::string{};
    }
}

/**
 * @brief Builds expanded location code from unexpanded location code.
 *
 * @param[in] i_unexpandedLocationCode - Unexpanded location code.
 * @param[in] i_isFcs - True if FCS location code, false if MTS.
 * @param[in] i_pos - Position where "fcs" or "mts" prefix was found.
 * @param[in] i_firstKwdValue - First keyword value (FC for FCS, TM for MTS).
 * @param[in] i_secondKwdValue - Second keyword value (SE).
 * @param[in] i_nodeIdentifier - Node identifier to embed in the expanded LC.
 *                               For FCS: "N00", "N01", etc. (derived from
 *                               chassis ID). Empty for MTS.
 *
 * @return Expanded location code string. In case of error, unexpanded is
 *         returned.
 */
inline std::string buildExpandedLc(
    const std::string& i_unexpandedLocationCode, bool i_isFcs, size_t i_pos,
    const std::string& i_firstKwdValue, const std::string& i_secondKwdValue,
    const std::string& i_nodeIdentifier) noexcept
{
    try
    {
        std::string l_expandedLC = i_unexpandedLocationCode;
        if (i_isFcs)
        {
            const std::string l_suffix = i_unexpandedLocationCode.substr(
                i_pos + constants::LOCATION_CODE_PREFIX_LENGTH);

            if (l_suffix.empty())
            {
                //"fcs"/"mts" is replaced with 4 bytes of VCEN:FC value.
                l_expandedLC.replace(
                    i_pos, constants::LOCATION_CODE_PREFIX_LENGTH,
                    i_firstKwdValue.substr(constants::FIRST_POSITION,
                                           constants::FC_KEYWORD_FIRST_4_BYTE) +
                        "." + i_nodeIdentifier + "." + i_secondKwdValue);
            }
            else if (l_suffix[constants::FIRST_POSITION] == '-')
            {
                    l_expandedLC.replace(
                        i_pos, constants::LOCATION_CODE_PREFIX_LENGTH,
                        i_firstKwdValue.substr(
                            constants::FIRST_POSITION,
                            constants::FC_KEYWORD_FIRST_4_BYTE) +
                            "." + i_nodeIdentifier + "." + i_secondKwdValue);
            }
        }
        else
        {
            // MTS: replace dashes in TM value with dots.
            std::string l_firstKwdValueCopy = i_firstKwdValue;
            std::replace(l_firstKwdValueCopy.begin(), l_firstKwdValueCopy.end(),
                         '-', '.');
            l_expandedLC.replace(i_pos, constants::LOCATION_CODE_PREFIX_LENGTH,
                                 l_firstKwdValueCopy + "." + i_secondKwdValue);
        }

        return l_expandedLC;
    }
    catch (const std::exception& l_ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "buildExpandedLc failed for unexpandedLocationCode: {}. Reason: {}",
            i_unexpandedLocationCode, l_ex.what()));

        return i_unexpandedLocationCode;
    }
}

/**
 * @brief Expands an FCS-type unexpanded location code.
 *
 * Tries to read FC and SE keywords from D-Bus (vcenInf) first:
 * - If the object mapper returns a non-empty result, FC and SE are read from
 *   D-Bus. Any keyword read failure returns an error immediately.
 * - If the object mapper returns empty, FC and SE are read from the parsed
 *   VPD map (VCEN record). Any keyword read failure returns an error
 *   immediately.
 *
 * @param[in] i_inventoryPath - Inventory Path.
 * @param[in] i_unexpandedLocationCode - Unexpanded location code.
 * @param[in] i_parsedVpdMap - Parsed VPD map.
 * @param[in] i_pos - Position of "fcs" in the unexpanded location code.
 *
 * @return std::expected with expanded location code on success, or error code
 *         on failure.
 * @throw Any exception is propagated to the caller.
 */
inline std::expected<std::string, uint16_t> getFcsExpandedLc(
    const std::string& i_inventoryPath,
    const std::string& i_unexpandedLocationCode,
    const types::VPDMapVariant& i_parsedVpdMap, size_t i_pos)
{
    if (i_inventoryPath.empty())
    {
        return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
    }

    uint16_t l_errCode{0};

    const auto l_chassisId = getChassisId(i_inventoryPath, l_errCode);
    if (l_errCode)
    {
        return std::unexpected(l_errCode);
    }

    if (l_chassisId.empty())
    {
        // Implies "/chassis" not found in inventory path.
        return std::unexpected(INVALID_INVENTORY_PATH);
    }

    const std::string l_chassisInvPath =
        std::format("{}/{}", constants::systemVpdInvPath, l_chassisId);

    const auto l_mapperRetValue =
        dbusUtility::getObjectMap(l_chassisInvPath, {constants::vcenInf});

    std::string l_fcKwdValue;
    std::string l_seKwdValue;

    if (!l_mapperRetValue.empty())
    {
        // Object mapper returned a result — read FC and SE from D-Bus.
        const std::string& l_service = l_mapperRetValue.begin()->first;

        const auto readKwd = [&](const std::string& i_kwd) -> std::string {
            const auto l_retVal = dbusUtility::readDbusProperty(
                l_service, l_chassisInvPath, constants::vcenInf, i_kwd);

            if (const auto l_val = std::get_if<types::BinaryVector>(&l_retVal))
            {
                return std::string(reinterpret_cast<const char*>(l_val->data()),
                                   l_val->size());
            }

            l_errCode = error_code::RECEIVED_INVALID_KWD_TYPE_FROM_DBUS;
            Logger::getLoggerInstance()->logMessage(
                std::format("Failed to read kwd {} from Dbus", i_kwd));

            return {};
        };

        l_fcKwdValue = readKwd(constants::kwdFC);
        if (l_errCode)
        {
            return std::unexpected(l_errCode);
        }

        l_seKwdValue = readKwd(constants::kwdSE);
        if (l_errCode)
        {
            return std::unexpected(l_errCode);
        }

        if (l_fcKwdValue.empty() || l_seKwdValue.empty())
        {
            return std::unexpected(error_code::INVALID_VALUE_READ_FROM_DBUS);
        }

        // FC keyword must be at least 4 characters for a valid substr.
        if (l_fcKwdValue.size() < constants::FC_KEYWORD_FIRST_4_BYTE)
        {
            Logger::getLoggerInstance()->logMessage(std::format(
                "FC keyword value '{}' read from Dbus, is too short (expected at least 4 characters)",
                l_fcKwdValue));

            return std::unexpected(error_code::INVALID_KEYWORD_LENGTH);
        }
    }
    else
    {
        // Object mapper returned empty — fall back to VPD map (VCEN record).
        const auto l_ipzMap = std::get_if<types::IPZVpdMap>(&i_parsedVpdMap);
        if (!l_ipzMap)
        {
            return std::unexpected(error_code::UNSUPPORTED_VPD_TYPE);
        }

        const auto l_vcenItr = l_ipzMap->find(constants::recVCEN);
        if (l_vcenItr == l_ipzMap->end())
        {
            return std::unexpected(error_code::RECORD_NOT_FOUND);
        }

        l_fcKwdValue = getKwVal(l_vcenItr->second, constants::kwdFC, l_errCode);
        if (l_errCode)
        {
            return std::unexpected(l_errCode);
        }

        l_seKwdValue = getKwVal(l_vcenItr->second, constants::kwdSE, l_errCode);
        if (l_errCode)
        {
            return std::unexpected(l_errCode);
        }

        if (l_fcKwdValue.empty() || l_seKwdValue.empty())
        {
            return std::unexpected(error_code::INVALID_VALUE_READ_FROM_EEPROM);
        }

        // FC keyword must be at least 4 characters for a valid substr.
        if (l_fcKwdValue.size() < constants::FC_KEYWORD_FIRST_4_BYTE)
        {
            Logger::getLoggerInstance()->logMessage(std::format(
                "FC keyword value '{}' read from EEPROM, is too short (expected at least 4 characters)",
                l_fcKwdValue));

            return std::unexpected(error_code::INVALID_KEYWORD_LENGTH);
        }
    }

    const std::string l_nodeNumber =
        l_chassisId.substr(std::string_view{"chassis"}.size());

    std::string l_nodeIdentifier;
    if (l_nodeNumber.empty())
    {
        // Legacy object paths carry no numeric suffix on the chassis segment
        // (e.g. ".../chassis/..."). Default to "N00" so that location-code
        // expansion still succeeds for such systems.
        l_nodeIdentifier = "N00";
    }
    else if (l_nodeNumber == "0")
    {
        l_nodeIdentifier = "SC0";
    }
    else
    {
        l_nodeIdentifier = "N" + (l_nodeNumber.size() == 1 ? "0" + l_nodeNumber
                                                           : l_nodeNumber);
    }

    return buildExpandedLc(i_unexpandedLocationCode, true, i_pos, l_fcKwdValue,
                           l_seKwdValue, l_nodeIdentifier);
}

/**
 * @brief Expands an MTS-type unexpanded location code.
 *
 * Reads VSYS-TM and VSYS-SE value from the parsed IPZ VPD map and calls
 * buildExpandedLc. Any exception is propagated to the caller.
 *
 * @param[in] i_unexpandedLocationCode - Unexpanded location code.
 * @param[in] i_parsedVpdMap - Parsed VPD map.
 * @param[in] i_pos - Position of "mts" in the unexpanded location code.
 *
 * @return std::expected with expanded location code on success, or error code
 *         on failure.
 *
 * @throw exception in case of failure. Caller needs to handle.
 */
inline std::expected<std::string, uint16_t> getMtsExpandedLc(
    const std::string& i_unexpandedLocationCode,
    const types::VPDMapVariant& i_parsedVpdMap, size_t i_pos)
{
    uint16_t l_errCode{0};

    const auto l_ipzMap = std::get_if<types::IPZVpdMap>(&i_parsedVpdMap);
    if (!l_ipzMap)
    {
        return std::unexpected(error_code::UNSUPPORTED_VPD_TYPE);
    }

    const auto l_recItr = l_ipzMap->find(constants::recVSYS);
    if (l_recItr == l_ipzMap->end())
    {
        return std::unexpected(error_code::RECORD_NOT_FOUND);
    }

    const std::string l_tmKwdValue =
        getKwVal(l_recItr->second, constants::kwdTM, l_errCode);
    if (l_errCode)
    {
        return std::unexpected(l_errCode);
    }

    const std::string l_seKwdValue =
        getKwVal(l_recItr->second, constants::kwdSE, l_errCode);
    if (l_errCode)
    {
        return std::unexpected(l_errCode);
    }

    if (l_tmKwdValue.empty() || l_seKwdValue.empty())
    {
        return std::unexpected(error_code::INVALID_VALUE_READ_FROM_EEPROM);
    }

    return buildExpandedLc(i_unexpandedLocationCode, false, i_pos, l_tmKwdValue,
                           l_seKwdValue, {});
}

/**
 * @brief API to expand unexpanded location code.
 *
 * Detects whether the location code is FCS or MTS type and delegates
 * expansion to getFcsExpandedLc or getMtsExpandedLc respectively.
 *
 * @param[in] i_inventoryPath - Inventory Path.
 * @param[in] i_unexpandedLocationCode - Unexpanded location code.
 * @param[in] i_parsedVpdMap - Parsed VPD map.
 *
 * @return std::expected with expanded location code on success, or error code
 *         on failure.
 */
inline std::expected<std::string, uint16_t> getExpandedLocationCode(
    const std::string& i_inventoryPath,
    const std::string& i_unexpandedLocationCode,
    const types::VPDMapVariant& i_parsedVpdMap) noexcept
{
    if (i_unexpandedLocationCode.empty())
    {
        return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
    }

    try
    {
        size_t l_pos = i_unexpandedLocationCode.find(constants::fcsTypeLc);
        if (l_pos != std::string::npos)
        {
            return getFcsExpandedLc(i_inventoryPath, i_unexpandedLocationCode,
                                    i_parsedVpdMap, l_pos);
        }

        l_pos = i_unexpandedLocationCode.find(constants::mtsTypeLc);
        if (l_pos != std::string::npos)
        {
            return getMtsExpandedLc(i_unexpandedLocationCode, i_parsedVpdMap,
                                    l_pos);
        }

        return std::unexpected(error_code::FAILED_TO_DETECT_LOCATION_CODE_TYPE);
    }
    catch (const std::exception& l_ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "getExpandedLocationCode failed for FRU: {}, unexpandedLocationCode: {}. Reason: {}",
            i_inventoryPath, i_unexpandedLocationCode, l_ex.what()));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

/**
 * @brief Update keyword value on D-Bus for all associated inventory paths.
 *
 * This API updates the keyword value on DBus for all inventory paths
 * associated with the given FRU path. It iterates through all inventory paths
 * for the given EEPROM path and updates the property if:
 * - The "inherit" tag is true for the inventory path, OR
 * - The record is part of the "copyRecords" tag and not in "skipRecords" list
 *
 * @param[in] i_fruPath - EEPROM path of the FRU.
 * @param[in] i_paramsToWriteData - Write VPD parameters
 * @param[in] i_sysCfgJsonObj - Config JSON object.
 * @param[out] o_errCode - To set error code incase of failure.
 */
inline void updateKeywordOnDBus(
    const std::string& i_fruPath,
    const types::WriteVpdParams& i_paramsToWriteData,
    const nlohmann::json& i_sysCfgJsonObj, uint16_t& o_errCode)
{
    o_errCode = 0;
    try
    {
        if (i_fruPath.empty())
        {
            o_errCode = error_code::INVALID_INPUT_PARAMETER;
            return;
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            o_errCode = error_code::INVALID_JSON;
            return;
        }

        if (!i_sysCfgJsonObj["frus"].contains(i_fruPath))
        {
            o_errCode = error_code::FRU_PATH_NOT_FOUND;
            return;
        }

        const types::IpzData* l_ipzData =
            std::get_if<types::IpzData>(&i_paramsToWriteData);

        // @todo Add support for other VPD types.
        if (!l_ipzData)
        {
            o_errCode = error_code::UNSUPPORTED_VPD_TYPE;
            return;
        }

        //  iterate through all inventory paths for given EEPROM path,
        //  if for an inventory path, "inherit" tag is true OR the record
        //  is part of "copyRecords" tag, update the inventory path's
        //  com.ibm.ipzvpd.<record>, property

        types::ObjectMap l_objectInterfaceMap;

        auto l_populateInterfaceMap = [&l_objectInterfaceMap,
                                       &l_ipzData = std::as_const(l_ipzData)](
                                          const auto& l_inventoryItem) {
            if (!l_ipzData)
            {
                return;
            }

            // check if record is part of copyRecords list.
            const bool l_isPartOfCopyRecord =
                l_inventoryItem.contains("copyRecords") &&
                std::find(l_inventoryItem["copyRecords"].begin(),
                          l_inventoryItem["copyRecords"].end(),
                          std::get<0>(*l_ipzData)) !=
                    l_inventoryItem["copyRecords"].end();

            // check if record is part of skipRecords list
            const bool l_isPartOfSkipRecord =
                l_inventoryItem.contains("skipRecords") &&
                std::find(l_inventoryItem["skipRecords"].begin(),
                          l_inventoryItem["skipRecords"].end(),
                          std::get<0>(*l_ipzData)) !=
                    l_inventoryItem["skipRecords"].end();

            // update for inherited FRUs or if the record is part of
            // copyRecords tag
            if (l_inventoryItem.value("inherit", true) ||
                (l_isPartOfCopyRecord && !l_isPartOfSkipRecord))
            {
                l_objectInterfaceMap.emplace(
                    sdbusplus::object_path{l_inventoryItem["inventoryPath"]},
                    types::InterfaceMap{
                        {constants::ipzVpdInf + std::get<0>(*l_ipzData),
                         types::PropertyMap{{std::get<1>(*l_ipzData),
                                             std::get<2>(*l_ipzData)}}}});
            }
        };

        // iterate through all inventory paths of the FRU.
        std::for_each(i_sysCfgJsonObj["frus"][i_fruPath].begin(),
                      i_sysCfgJsonObj["frus"][i_fruPath].end(),
                      l_populateInterfaceMap);

        if (!l_objectInterfaceMap.empty())
        {
            if (!dbusUtility::publishVpdOnDBus(std::move(l_objectInterfaceMap)))
            {
                o_errCode = error_code::DBUS_FAILURE;
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        o_errCode = error_code::STANDARD_EXCEPTION;

        std::string l_recordKeywordInfo;
        const types::IpzData* l_ipzData =
            std::get_if<types::IpzData>(&i_paramsToWriteData);

        // @todo Add support for other VPD types.
        if (l_ipzData)
        {
            l_recordKeywordInfo =
                std::format("[{}] : [{}]", std::get<0>(*l_ipzData),
                            std::get<1>(*l_ipzData));
        }

        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to update keyword [{}] on DBus for path [{}], error : {}.",
            l_recordKeywordInfo, i_fruPath, l_ex.what()));
    }
}
} // namespace vpdSpecificUtility
} // namespace vpd
