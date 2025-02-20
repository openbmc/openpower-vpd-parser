#pragma once

#include "config.h"

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>
#include <utility/common_utility.hpp>
#include <utility/dbus_utility.hpp>

#include <filesystem>
#include <fstream>
#include <regex>

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
 *
 * @return On success, returns generated file name, otherwise returns empty
 * string.
 */
inline std::string generateBadVPDFileName(
    const std::string& i_vpdFilePath) noexcept
{
    std::string l_badVpdFileName{BAD_VPD_DIR};
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
        logging::logMessage("Failed to generate bad VPD file name for [" +
                            i_vpdFilePath + "]. Error: " + l_ex.what());
    }
    return l_badVpdFileName;
}

/**
 * @brief API which dumps the broken/bad vpd in a directory.
 * When the vpd is bad, this API places  the bad vpd file inside
 * "/tmp/bad-vpd" in BMC, in order to collect bad VPD data as a part of user
 * initiated BMC dump.
 *
 *
 * @param[in] i_vpdFilePath - vpd file path
 * @param[in] i_vpdVector - vpd vector
 *
 * @return On success returns 0, otherwise returns -1.
 */
inline int dumpBadVpd(const std::string& i_vpdFilePath,
                      const types::BinaryVector& i_vpdVector) noexcept
{
    int l_rc{constants::FAILURE};
    try
    {
        std::filesystem::create_directory(BAD_VPD_DIR);
        auto l_badVpdPath = generateBadVPDFileName(i_vpdFilePath);

        if (l_badVpdPath.empty())
        {
            throw std::runtime_error("Failed to generate bad VPD file name");
        }

        if (std::filesystem::exists(l_badVpdPath))
        {
            std::error_code l_ec;
            std::filesystem::remove(l_badVpdPath, l_ec);
            if (l_ec) // error code
            {
                const std::string l_errorMsg{
                    "Error removing the existing broken vpd in " +
                    l_badVpdPath +
                    ". Error code : " + std::to_string(l_ec.value()) +
                    ". Error message : " + l_ec.message()};

                throw std::runtime_error(l_errorMsg);
            }
        }

        std::ofstream l_badVpdFileStream(l_badVpdPath, std::ofstream::binary);
        if (!l_badVpdFileStream.is_open())
        {
            throw std::runtime_error(
                "Failed to open bad vpd file path in /tmp/bad-vpd. "
                "Unable to dump the broken/bad vpd file.");
        }

        l_badVpdFileStream.write(
            reinterpret_cast<const char*>(i_vpdVector.data()),
            i_vpdVector.size());

        l_rc = constants::SUCCESS;
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to dump bad VPD for [" + i_vpdFilePath +
                            "]. Error: " + l_ex.what());
    }
    return l_rc;
}

/**
 * @brief An API to read value of a keyword.
 *
 *
 * @param[in] i_kwdValueMap - A map having Kwd value pair.
 * @param[in] i_kwd - keyword name.
 *
 * @return On success returns value of the keyword read from map, otherwise
 * returns empty string.
 */
inline std::string getKwVal(const types::IPZKwdValueMap& i_kwdValueMap,
                            const std::string& i_kwd) noexcept
{
    std::string l_kwdValue;
    try
    {
        if (i_kwd.empty())
        {
            throw std::runtime_error("Invalid parameters");
        }

        auto l_itrToKwd = i_kwdValueMap.find(i_kwd);
        if (l_itrToKwd != i_kwdValueMap.end())
        {
            l_kwdValue = l_itrToKwd->second;
        }
    }
    catch (const std::exception& l_ex)
    {
        l_kwdValue.clear();
        logging::logMessage("Failed to get keyword value for [" + i_kwd +
                            "]. Error : " + l_ex.what());
    }
    return l_kwdValue;
}

/**
 * @brief An API to process encoding of a keyword.
 *
 * @param[in] i_keyword - Keyword to be processed.
 * @param[in] i_encoding - Type of encoding.
 *
 * @return Value after being processed for encoded type.
 */
inline std::string encodeKeyword(const std::string& i_keyword,
                                 const std::string& i_encoding) noexcept
{
    // Default value is keyword value
    std::string l_result(i_keyword.begin(), i_keyword.end());
    try
    {
        if (i_encoding == "MAC")
        {
            l_result.clear();
            size_t l_firstByte = i_keyword[0];
            l_result += commonUtility::toHex(l_firstByte >> 4);
            l_result += commonUtility::toHex(l_firstByte & 0x0f);
            for (size_t i = 1; i < i_keyword.size(); ++i)
            {
                l_result += ":";
                l_result += commonUtility::toHex(i_keyword[i] >> 4);
                l_result += commonUtility::toHex(i_keyword[i] & 0x0f);
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
        logging::logMessage("Failed to encode keyword [" + i_keyword +
                            "]. Error: " + l_ex.what());
    }

    return l_result;
}

/**
 * @brief Helper function to insert or merge in map.
 *
 * This method checks in an interface if the given interface exists. If the
 * interface key already exists, property map is inserted corresponding to it.
 * If the key does'nt exist then given interface and property map pair is newly
 * created. If the property present in propertymap already exist in the
 * InterfaceMap, then the new property value is ignored.
 *
 * @param[in,out] io_map - Interface map.
 * @param[in] i_interface - Interface to be processed.
 * @param[in] i_propertyMap - new property map that needs to be emplaced.
 *
 * @return On success returns 0, otherwise returns -1.
 */
inline int insertOrMerge(types::InterfaceMap& io_map,
                         const std::string& i_interface,
                         types::PropertyMap&& i_propertyMap) noexcept
{
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
        // ToDo:: Log PEL
        logging::logMessage(
            "Inserting properties into interface[" + i_interface +
            "] map failed, reason: " + std::string(l_ex.what()));
    }
    return l_rc;
}

/**
 * @brief API to expand unpanded location code.
 *
 * Note: The API handles all the exception internally, in case of any error
 * unexpanded location code will be returned as it is.
 *
 * @param[in] unexpandedLocationCode - Unexpanded location code.
 * @param[in] parsedVpdMap - Parsed VPD map.
 * @return Expanded location code. In case of any error, unexpanded is returned
 * as it is.
 */
inline std::string getExpandedLocationCode(
    const std::string& unexpandedLocationCode,
    const types::VPDMapVariant& parsedVpdMap)
{
    auto expanded{unexpandedLocationCode};

    try
    {
        // Expanded location code is formed by combining two keywords
        // depending on type in unexpanded one. Second one is always "SE".
        std::string kwd1, kwd2{constants::kwdSE};

        // interface to search for required keywords;
        std::string kwdInterface;

        // record which holds the required keywords.
        std::string recordName;

        auto pos = unexpandedLocationCode.find("fcs");
        if (pos != std::string::npos)
        {
            kwd1 = constants::kwdFC;
            kwdInterface = constants::vcenInf;
            recordName = constants::recVCEN;
        }
        else
        {
            pos = unexpandedLocationCode.find("mts");
            if (pos != std::string::npos)
            {
                kwd1 = constants::kwdTM;
                kwdInterface = constants::vsysInf;
                recordName = constants::recVSYS;
            }
            else
            {
                throw std::runtime_error(
                    "Error detecting type of unexpanded location code.");
            }
        }

        std::string firstKwdValue, secondKwdValue;

        if (auto ipzVpdMap = std::get_if<types::IPZVpdMap>(&parsedVpdMap);
            ipzVpdMap && (*ipzVpdMap).find(recordName) != (*ipzVpdMap).end())
        {
            auto itrToVCEN = (*ipzVpdMap).find(recordName);
            // The exceptions will be cautght at end.
            firstKwdValue = getKwVal(itrToVCEN->second, kwd1);
            secondKwdValue = getKwVal(itrToVCEN->second, kwd2);
        }
        else
        {
            std::array<const char*, 1> interfaceList = {kwdInterface.c_str()};

            types::MapperGetObject mapperRetValue = dbusUtility::getObjectMap(
                std::string(constants::systemVpdInvPath), interfaceList);

            if (mapperRetValue.empty())
            {
                throw std::runtime_error("Mapper failed to get service");
            }

            const std::string& serviceName = std::get<0>(mapperRetValue.at(0));

            auto retVal = dbusUtility::readDbusProperty(
                serviceName, std::string(constants::systemVpdInvPath),
                kwdInterface, kwd1);

            if (auto kwdVal = std::get_if<types::BinaryVector>(&retVal))
            {
                firstKwdValue.assign(
                    reinterpret_cast<const char*>(kwdVal->data()),
                    kwdVal->size());
            }
            else
            {
                throw std::runtime_error(
                    "Failed to read value of " + kwd1 + " from Bus");
            }

            retVal = dbusUtility::readDbusProperty(
                serviceName, std::string(constants::systemVpdInvPath),
                kwdInterface, kwd2);

            if (auto kwdVal = std::get_if<types::BinaryVector>(&retVal))
            {
                secondKwdValue.assign(
                    reinterpret_cast<const char*>(kwdVal->data()),
                    kwdVal->size());
            }
            else
            {
                throw std::runtime_error(
                    "Failed to read value of " + kwd2 + " from Bus");
            }
        }

        if (unexpandedLocationCode.find("fcs") != std::string::npos)
        {
            // TODO: See if ND0 can be placed in the JSON
            expanded.replace(
                pos, 3, firstKwdValue.substr(0, 4) + ".ND0." + secondKwdValue);
        }
        else
        {
            replace(firstKwdValue.begin(), firstKwdValue.end(), '-', '.');
            expanded.replace(pos, 3, firstKwdValue + "." + secondKwdValue);
        }
    }
    catch (const std::exception& ex)
    {
        logging::logMessage("Failed to expand location code with exception: " +
                            std::string(ex.what()));
    }

    return expanded;
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
 */
inline void getVpdDataInVector(const std::string& vpdFilePath,
                               types::BinaryVector& vpdVector,
                               size_t& vpdStartOffset)
{
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
        std::cerr << "Exception in file handling [" << vpdFilePath
                  << "] error : " << fail.what();
        throw;
    }
}

/**
 * @brief An API to get D-bus representation of given VPD keyword.
 *
 * @param[in] i_keywordName - VPD keyword name.
 *
 * @return D-bus representation of given keyword.
 */
inline std::string getDbusPropNameForGivenKw(const std::string& i_keywordName)
{
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
 * @throw std::runtime_error
 * @throw DataException
 *
 * @param[in] i_JsonObject - Any JSON which contains CCIN tag to match.
 * @param[in] i_parsedVpdMap - Parsed VPD map.
 * @return True if found, false otherwise.
 */
inline bool findCcinInVpd(const nlohmann::json& i_JsonObject,
                          const types::VPDMapVariant& i_parsedVpdMap)
{
    if (i_JsonObject.empty())
    {
        throw std::runtime_error("Json object is empty. Can't find CCIN");
    }

    if (auto l_ipzVPDMap = std::get_if<types::IPZVpdMap>(&i_parsedVpdMap))
    {
        auto l_itrToRec = (*l_ipzVPDMap).find("VINI");
        if (l_itrToRec == (*l_ipzVPDMap).end())
        {
            throw DataException(
                "VINI record not found in parsed VPD. Can't find CCIN");
        }

        std::string l_ccinFromVpd{
            vpdSpecificUtility::getKwVal(l_itrToRec->second, "CC")};
        if (l_ccinFromVpd.empty())
        {
            throw DataException("Empty CCIN value in VPD map. Can't find CCIN");
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
                return true;
            }
        }

        logging::logMessage("No match found for CCIN");
        return false;
    }

    logging::logMessage("VPD type not supported. Can't find CCIN");
    return false;
}

/**
 * @brief API to reset data of a FRU populated under PIM.
 *
 * This API resets the data for particular interfaces of a FRU under PIM.
 *
 * @param[in] i_objectPath - DBus object path of the FRU.
 * @param[in] io_interfaceMap - Interface and its properties map.
 */
inline void resetDataUnderPIM(const std::string& i_objectPath,
                              types::InterfaceMap& io_interfaceMap)
{
    try
    {
        std::array<const char*, 0> l_interfaces;
        const types::MapperGetObject& l_getObjectMap =
            dbusUtility::getObjectMap(i_objectPath, l_interfaces);

        const std::vector<std::string>& l_vpdRelatedInterfaces{
            constants::operationalStatusInf, constants::inventoryItemInf,
            constants::assetInf};

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
                     std::string::npos) ||
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
                            l_propertyMap.emplace(l_propertyName,
                                                  std::string{});
                        }
                        else if (std::holds_alternative<bool>(l_propertyValue))
                        {
                            // ToDo -- Update the functional status property
                            // to true.
                            if (l_propertyName.compare("Present") ==
                                constants::STR_CMP_SUCCESS)
                            {
                                l_propertyMap.emplace(l_propertyName, false);
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
        logging::logMessage("Failed to remove VPD for FRU: " + i_objectPath +
                            " with error: " + std::string(l_ex.what()));
    }
}

/**
 * @brief API to detect pass1 planar type.
 *
 * Based on HW version and IM keyword, This API detects is it is a pass1 planar
 * or not.
 *
 * @return True if pass 1 planar, false otherwise.
 */
inline bool isPass1Planar()
{
    auto l_retVal = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::viniInf, constants::kwdHW);

    auto l_hwVer = std::get_if<types::BinaryVector>(&l_retVal);

    l_retVal = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemInvPath, constants::vsbpInf,
        constants::kwdIM);

    auto l_imValue = std::get_if<types::BinaryVector>(&l_retVal);

    if (l_hwVer && l_imValue)
    {
        types::BinaryVector everest{80, 00, 48, 00};
        types::BinaryVector fuji{96, 00, 32, 00};

        if (((*l_imValue) == everest) || ((*l_imValue) == fuji))
        {
            if ((*l_hwVer).at(1) < constants::VALUE_21)
            {
                return true;
            }
        }
        else if ((*l_hwVer).at(1) < constants::VALUE_2)
        {
            return true;
        }
    }

    return false;
}
} // namespace vpdSpecificUtility
} // namespace vpd
