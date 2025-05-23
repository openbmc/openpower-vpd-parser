#pragma once

#include "config.h"

#include "constants.hpp"
#include "event_logger.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>
#include <utility/common_utility.hpp>
#include <utility/dbus_utility.hpp>

#include <filesystem>
#include <fstream>
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
 *
 * @return On success, returns generated file name, otherwise returns empty
 * string.
 */
inline std::string generateBadVPDFileName(
    const std::string& i_vpdFilePath) noexcept
{
    std::string l_badVpdFileName{constants::badVpdDir};
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
 * "/var/lib/vpd/dumps" in BMC, in order to collect bad VPD data as a part of
 * user initiated BMC dump.
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
        std::filesystem::create_directory(constants::badVpdDir);
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
            const std::string l_errorMsg{
                "Failed to open bad vpd file path [" + l_badVpdPath +
                "]. Unable to dump the broken/bad vpd file."};

            throw std::runtime_error(l_errorMsg);
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
        else
        {
            throw std::runtime_error("Keyword not found");
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to get value for keyword [" + i_kwd +
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
            firstKwdValue = getKwVal(itrToVCEN->second, kwd1);
            if (firstKwdValue.empty())
            {
                throw std::runtime_error(
                    "Failed to get value for keyword [" + kwd1 + "]");
            }

            secondKwdValue = getKwVal(itrToVCEN->second, kwd2);
            if (secondKwdValue.empty())
            {
                throw std::runtime_error(
                    "Failed to get value for keyword [" + kwd2 + "]");
            }
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
 * @param[in] i_JsonObject - Any JSON which contains CCIN tag to match.
 * @param[in] i_parsedVpdMap - Parsed VPD map.
 *
 * @return True if found, false otherwise.
 */
inline bool findCcinInVpd(const nlohmann::json& i_JsonObject,
                          const types::VPDMapVariant& i_parsedVpdMap) noexcept
{
    bool l_rc{false};
    try
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
                throw DataException(
                    "Empty CCIN value in VPD map. Can't find CCIN");
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
            logging::logMessage("VPD type not supported. Can't find CCIN");
        }
    }
    catch (const std::exception& l_ex)
    {
        const std::string l_errMsg{
            "Failed to find CCIN in VPD. Error : " + std::string(l_ex.what())};

        if (typeid(l_ex) == std::type_index(typeid(DataException)))
        {
            EventLogger::createSyncPel(
                types::ErrorType::InvalidVpdMessage,
                types::SeverityType::Informational, __FILE__, __FUNCTION__, 0,
                l_errMsg, std::nullopt, std::nullopt, std::nullopt,
                std::nullopt);
        }

        logging::logMessage(l_errMsg);
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
                            if (l_propertyName.compare("CollectionStatus") ==
                                constants::STR_CMP_SUCCESS)
                            {
                                l_propertyMap.emplace(
                                    l_propertyName,
                                    constants::vpdCollectionNotStarted);
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
                                l_propertyMap.emplace(l_propertyName, false);
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
inline bool isPass1Planar() noexcept
{
    bool l_rc{false};
    try
    {
        auto l_retVal = dbusUtility::readDbusProperty(
            constants::pimServiceName, constants::systemVpdInvPath,
            constants::viniInf, constants::kwdHW);

        auto l_hwVer = std::get_if<types::BinaryVector>(&l_retVal);

        l_retVal = dbusUtility::readDbusProperty(
            constants::pimServiceName, constants::systemInvPath,
            constants::vsbpInf, constants::kwdIM);

        auto l_imValue = std::get_if<types::BinaryVector>(&l_retVal);

        if (l_hwVer && l_imValue)
        {
            if (l_hwVer->size() != constants::VALUE_2)
            {
                throw std::runtime_error("Invalid HW keyword length.");
            }

            if (l_imValue->size() != constants::VALUE_4)
            {
                throw std::runtime_error("Invalid IM keyword length.");
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
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage("Failed to check for pass 1 planar. Error: " +
                            std::string(l_ex.what()));
    }

    return l_rc;
}

/**
 * @brief API to detect if system configuration is that of PowerVS system.
 *
 * @param[in] i_imValue - IM value of the system.
 * @return true if it is PowerVS configuration, false otherwise.
 */
inline bool isPowerVsConfiguration(const types::BinaryVector& i_imValue)
{
    if (i_imValue.empty() || i_imValue.size() != constants::VALUE_4)
    {
        return false;
    }

    // Should be a 0x5000XX series system.
    if (i_imValue.at(0) == constants::HEX_VALUE_50 &&
        i_imValue.at(1) == constants::HEX_VALUE_00)
    {
        std::string l_imagePrefix = dbusUtility::getImagePrefix();

        // Check image for 0x500030XX series.
        if ((i_imValue.at(2) == constants::HEX_VALUE_30) &&
            ((l_imagePrefix == constants::powerVsImagePrefix_MY) ||
             (l_imagePrefix == constants::powerVsImagePrefix_NY)))
        {
            logging::logMessage("PowerVS configuration");
            return true;
        }

        // Check image for 0X500010XX series.
        if ((i_imValue.at(2) == constants::HEX_VALUE_10) &&
            ((l_imagePrefix == constants::powerVsImagePrefix_MZ) ||
             (l_imagePrefix == constants::powerVsImagePrefix_NZ)))
        {
            logging::logMessage("PowerVS configuration");
            return true;
        }
    }
    return false;
}

/**
 * @brief API to get CCIN for a given FRU from DBus.
 *
 * The API reads the CCIN for a FRU based on its inventory path.
 *
 * @param[in] i_invObjPath - Inventory path of the FRU.
 * @return CCIN of the FRU on success, empty string otherwise.
 */
inline std::string getCcinFromDbus(const std::string& i_invObjPath)
{
    try
    {
        if (i_invObjPath.empty())
        {
            throw std::runtime_error("Empty EEPROM path, can't read CCIN");
        }

        const auto& l_retValue = dbusUtility::readDbusProperty(
            constants::pimServiceName, i_invObjPath, constants::viniInf,
            constants::kwdCCIN);

        auto l_ptrCcin = std::get_if<types::BinaryVector>(&l_retValue);
        if (!l_ptrCcin || (*l_ptrCcin).size() != constants::VALUE_4)
        {
            throw DbusException("Invalid CCIN read from Dbus");
        }

        return std::string((*l_ptrCcin).begin(), (*l_ptrCcin).end());
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(l_ex.what());
        return std::string{};
    }
}

/**
 * @brief API to check if the current running image is a powerVS image.
 *
 * @return true if it is PowerVS image, false otherwise.
 */
inline bool isPowerVsImage()
{
    std::string l_imagePrefix = dbusUtility::getImagePrefix();

    if ((l_imagePrefix == constants::powerVsImagePrefix_MY) ||
        (l_imagePrefix == constants::powerVsImagePrefix_NY) ||
        (l_imagePrefix == constants::powerVsImagePrefix_MZ) ||
        (l_imagePrefix == constants::powerVsImagePrefix_NZ))
    {
        return true;
    }
    return false;
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
 *
 */
inline void updateKwdOnInheritedFrus(
    const std::string& i_fruPath,
    const types::WriteVpdParams& i_paramsToWriteData,
    const nlohmann::json& i_sysCfgJsonObj) noexcept
{
    try
    {
        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error("Mandatory tag(s) missing from JSON");
        }

        if (!i_sysCfgJsonObj["frus"].contains(i_fruPath))
        {
            throw std::runtime_error(
                "VPD path [" + i_fruPath + "] not found in system config JSON");
        }

        const types::IpzData* l_ipzData =
            std::get_if<types::IpzData>(&i_paramsToWriteData);

        if (!l_ipzData)
        {
            throw std::runtime_error("Unsupported VPD type");
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
                        sdbusplus::message::object_path{l_Fru["inventoryPath"]},
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
            // notify PIM
            if (!dbusUtility::callPIM(move(l_objectInterfaceMap)))
            {
                throw std::runtime_error(
                    "Call to PIM failed for VPD file " + i_fruPath);
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to sync keyword update to inherited FRUs of FRU [" +
            i_fruPath + "]. Error: " + std::string(l_ex.what()));
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
 *
 * @return Returns a map of common interface(s) and properties corresponding to
 * the record and keyword. An empty map is returned if no such common
 * interface(s) and properties are found.
 */
inline types::InterfaceMap getCommonInterfaceProperties(
    const types::WriteVpdParams& i_paramsToWriteData,
    const nlohmann::json& i_commonInterfaceJson) noexcept
{
    types::InterfaceMap l_interfaceMap;
    try
    {
        const types::IpzData* l_ipzData =
            std::get_if<types::IpzData>(&i_paramsToWriteData);

        if (!l_ipzData)
        {
            throw std::runtime_error("Invalid VPD type");
        }

        auto l_populateInterfaceMap = [&l_ipzData = std::as_const(l_ipzData),
                                       &l_interfaceMap](
                                          const auto& l_interfacesPropPair) {
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
                // add property map to interface map
                l_interfaceMap.emplace(
                    l_interfacesPropPair.key(),
                    types::PropertyMap{
                        {l_matchPropValuePairIt.key(),
                         vpdSpecificUtility::encodeKeyword(
                             std::string(std::get<2>(*l_ipzData).begin(),
                                         std::get<2>(*l_ipzData).end()),
                             l_matchPropValuePairIt.value().value("encoding",
                                                                  ""))}});
            }
        };

        // iterate through all common interfaces and populate interface map
        std::for_each(i_commonInterfaceJson.items().begin(),
                      i_commonInterfaceJson.items().end(),
                      l_populateInterfaceMap);
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to find common interface properties. Error: " +
            std::string(l_ex.what()));
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
 *
 */
inline void updateCiPropertyOfInheritedFrus(
    const std::string& i_fruPath,
    const types::WriteVpdParams& i_paramsToWriteData,
    const nlohmann::json& i_sysCfgJsonObj) noexcept
{
    try
    {
        if (!i_sysCfgJsonObj.contains("commonInterfaces"))
        {
            // no common interfaces in JSON, nothing to do
            return;
        }

        if (!i_sysCfgJsonObj.contains("frus"))
        {
            throw std::runtime_error("Mandatory tag(s) missing from JSON");
        }

        if (!i_sysCfgJsonObj["frus"].contains(i_fruPath))
        {
            throw std::runtime_error(
                "VPD path [" + i_fruPath + "] not found in system config JSON");
        }

        if (!std::get_if<types::IpzData>(&i_paramsToWriteData))
        {
            throw std::runtime_error("Unsupported VPD type");
        }

        //  iterate through all inventory paths for given EEPROM path,
        //  if for an inventory path, "inherit" tag is true,
        //  update the inventory path's com.ibm.ipzvpd.<record>,keyword
        //  property

        types::ObjectMap l_objectInterfaceMap;

        const types::InterfaceMap l_interfaceMap = getCommonInterfaceProperties(
            i_paramsToWriteData, i_sysCfgJsonObj["commonInterfaces"]);

        if (l_interfaceMap.empty())
        {
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
                        sdbusplus::message::object_path{l_Fru["inventoryPath"]},
                        l_interfaceMap);
                }
            };

        std::for_each(i_sysCfgJsonObj["frus"][i_fruPath].begin(),
                      i_sysCfgJsonObj["frus"][i_fruPath].end(),
                      l_populateObjectInterfaceMap);

        if (!l_objectInterfaceMap.empty())
        {
            // notify PIM
            if (!dbusUtility::callPIM(move(l_objectInterfaceMap)))
            {
                throw std::runtime_error(
                    "Call to PIM failed for VPD file " + i_fruPath);
            }
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to update common interface properties of FRU [" +
            i_fruPath + "]. Error: " + std::string(l_ex.what()));
    }
}
} // namespace vpdSpecificUtility
} // namespace vpd
