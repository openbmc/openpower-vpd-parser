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

#include <algorithm>
#include <cctype>
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
 * @param[in] vpdFilePath - file path of the vpd.
 * @param[out] errCode - to set error code in case of error.
 *
 * @return On success, returns generated file name, otherwise returns empty
 * string.
 */
inline std::string generateBadVPDFileName(const std::string& vpdFilePath,
                                          uint16_t& errCode) noexcept
{
    errCode = 0;
    std::string badVpdFileName{constants::badVpdDir};

    if (vpdFilePath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return badVpdFileName;
    }

    try
    {
        if (vpdFilePath.find("i2c") != std::string::npos)
        {
            badVpdFileName += "i2c-";
            std::regex i2cPattern("(at24/)([0-9]+-[0-9]+)\\/");
            std::smatch match;
            if (std::regex_search(vpdFilePath, match, i2cPattern))
            {
                badVpdFileName += match.str(2);
            }
        }
        else if (vpdFilePath.find("spi") != std::string::npos)
        {
            std::regex spiPattern("((spi)[0-9]+)(.0)");
            std::smatch match;
            if (std::regex_search(vpdFilePath, match, spiPattern))
            {
                badVpdFileName += match.str(1);
            }
        }
    }
    catch (const std::exception& ex)
    {
        badVpdFileName.clear();
        errCode = error_code::STANDARD_EXCEPTION;
    }
    return badVpdFileName;
}

/**
 * @brief API which dumps the broken/bad vpd in a directory.
 * When the vpd is bad, this API places  the bad vpd file inside
 * "/var/lib/vpd/dumps" in BMC, in order to collect bad VPD data as a part of
 * user initiated BMC dump.
 *
 *
 * @param[in] vpdFilePath - vpd file path
 * @param[in] vpdVector - vpd vector
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success returns 0, otherwise returns -1.
 */
inline int dumpBadVpd(const std::string& vpdFilePath,
                      const types::BinaryVector& vpdVector,
                      uint16_t& errCode) noexcept
{
    errCode = 0;
    if (vpdFilePath.empty() || vpdVector.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return constants::FAILURE;
    }

    int rc{constants::FAILURE};
    try
    {
        std::filesystem::create_directory(constants::badVpdDir);
        auto badVpdPath = generateBadVPDFileName(vpdFilePath, errCode);

        if (badVpdPath.empty())
        {
            if (errCode)
            {
                Logger::getLoggerInstance()->logMessage(
                    "Failed to create bad VPD file name : " +
                    commonUtility::getErrCodeMsg(errCode));
            }

            return constants::FAILURE;
        }

        if (std::filesystem::exists(badVpdPath))
        {
            std::error_code ec;
            std::filesystem::remove(badVpdPath, ec);
            if (ec) // error code
            {
                errCode = error_code::FILE_SYSTEM_ERROR;
                return constants::FAILURE;
            }
        }

        std::ofstream badVpdFileStream(badVpdPath, std::ofstream::binary);
        if (!badVpdFileStream.is_open())
        {
            errCode = error_code::FILE_ACCESS_ERROR;
            return constants::FAILURE;
        }

        badVpdFileStream.write(reinterpret_cast<const char*>(vpdVector.data()),
                               vpdVector.size());

        rc = constants::SUCCESS;
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }
    return rc;
}

/**
 * @brief An API to read value of a keyword.
 *
 *
 * @param[in] kwdValueMap - A map having Kwd value pair.
 * @param[in] kwd - keyword name.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success returns value of the keyword read from map, otherwise
 * returns empty string.
 */
inline std::string getKwVal(const types::IPZKwdValueMap& kwdValueMap,
                            const std::string& kwd, uint16_t& errCode) noexcept
{
    errCode = 0;
    std::string kwdValue;
    if (kwd.empty() || kwdValueMap.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return kwdValue;
    }

    auto itrToKwd = kwdValueMap.find(kwd);
    if (itrToKwd != kwdValueMap.end())
    {
        kwdValue = itrToKwd->second;
    }
    else
    {
        errCode = error_code::KEYWORD_NOT_FOUND;
    }

    return kwdValue;
}

/**
 * @brief An API to process encoding of a keyword.
 *
 * @param[in] keyword - Keyword to be processed.
 * @param[in] encoding - Type of encoding.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return Value after being processed for encoded type.
 */
inline std::string encodeKeyword(const std::string& keyword,
                                 const std::string& encoding,
                                 uint16_t& errCode) noexcept
{
    errCode = 0;
    if (keyword.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }

    // Default value is keyword value
    std::string result(keyword.begin(), keyword.end());

    if (encoding.empty())
    {
        return result;
    }

    try
    {
        if (encoding == "MAC")
        {
            result.clear();
            size_t firstByte = keyword[0];

            auto hexValue = commonUtility::toHex(firstByte >> 4);

            if (!hexValue)
            {
                errCode = error_code::OUT_OF_BOUND_EXCEPTION;
                return std::string{};
            }

            result += hexValue;

            hexValue = commonUtility::toHex(firstByte & 0x0f);

            if (!hexValue)
            {
                errCode = error_code::OUT_OF_BOUND_EXCEPTION;
                return std::string{};
            }

            result += hexValue;

            for (size_t i = 1; i < keyword.size(); ++i)
            {
                result += ":";

                hexValue = commonUtility::toHex(keyword[i] >> 4);

                if (!hexValue)
                {
                    errCode = error_code::OUT_OF_BOUND_EXCEPTION;
                    return std::string{};
                }

                result += hexValue;

                hexValue = commonUtility::toHex(keyword[i] & 0x0f);

                if (!hexValue)
                {
                    errCode = error_code::OUT_OF_BOUND_EXCEPTION;
                    return std::string{};
                }

                result += hexValue;
            }
        }
        else if (encoding == "DATE")
        {
            // Date, represent as
            // <year>-<month>-<day> <hour>:<min>
            result.clear();
            static constexpr uint8_t skipPrefix = 3;

            auto strItr = keyword.begin();
            advance(strItr, skipPrefix);
            for_each(strItr, keyword.end(),
                     [&result](size_t c) { result += c; });

            result.insert(constants::BD_YEAR_END, 1, '-');
            result.insert(constants::BD_MONTH_END, 1, '-');
            result.insert(constants::BD_DAY_END, 1, ' ');
            result.insert(constants::BD_HOUR_END, 1, ':');
        }
    }
    catch (const std::exception& ex)
    {
        result.clear();
        errCode = error_code::STANDARD_EXCEPTION;
    }

    return result;
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
 * @param[in,out] map - Interface map.
 * @param[in] interface - Interface to be processed.
 * @param[in] propertyMap - new property map that needs to be emplaced.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success returns 0, otherwise returns -1.
 */
inline int insertOrMerge(types::InterfaceMap& map, const std::string& interface,
                         types::PropertyMap&& propertyMap,
                         uint16_t& errCode) noexcept
{
    errCode = 0;
    int rc{constants::FAILURE};

    try
    {
        if (map.find(interface) != map.end())
        {
            auto& prop = map.at(interface);
            std::for_each(propertyMap.begin(), propertyMap.end(),
                          [&prop](auto keyValue) {
                              prop[keyValue.first] = keyValue.second;
                          });
        }
        else
        {
            map.emplace(interface, propertyMap);
        }

        rc = constants::SUCCESS;
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }
    return rc;
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
 * @param[out] errCode - To set error code in case of error.
 */
inline void getVpdDataInVector(const std::string& vpdFilePath,
                               types::BinaryVector& vpdVector,
                               size_t& vpdStartOffset, uint16_t& errCode)
{
    errCode = 0;
    if (vpdFilePath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
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
        errCode = error_code::FILE_SYSTEM_ERROR;
        return;
    }
}

/**
 * @brief An API to get D-bus representation of given VPD keyword.
 *
 * @param[in] keywordName - VPD keyword name.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return D-bus representation of given keyword.
 */
inline std::string getDbusPropNameForGivenKw(const std::string& keywordName,
                                             uint16_t& errCode)
{
    errCode = 0;
    if (keywordName.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return std::string{};
    }
    // Check for "#" prefixed VPD keyword.
    if ((keywordName.size() == vpd::constants::TWO_BYTES) &&
        (keywordName.at(0) == constants::POUND_KW))
    {
        // D-bus doesn't support "#". Replace "#" with "PD_" for those "#"
        // prefixed keywords.
        return (std::string(constants::POUND_KW_PREFIX) +
                keywordName.substr(1));
    }

    // Return the keyword name back, if D-bus representation is same as the VPD
    // keyword name.
    return keywordName;
}

/**
 * @brief API to find CCIN in parsed VPD map.
 *
 * Few FRUs need some special handling. To identify those FRUs CCIN are used.
 * The API will check from parsed VPD map if the FRU is the one with desired
 * CCIN.
 *
 * @param[in] jsonObject - Any JSON which contains CCIN tag to match.
 * @param[in] parsedVpdMap - Parsed VPD map.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return True if found, false otherwise.
 */
inline bool findCcinInVpd(const nlohmann::json& jsonObject,
                          const types::VPDMapVariant& parsedVpdMap,
                          uint16_t& errCode) noexcept
{
    errCode = 0;
    bool rc{false};
    try
    {
        if (jsonObject.empty() ||
            std::holds_alternative<std::monostate>(parsedVpdMap))
        {
            errCode = error_code::INVALID_INPUT_PARAMETER;
            return rc;
        }

        if (auto ipzVPDMap = std::get_if<types::IPZVpdMap>(&parsedVpdMap))
        {
            auto itrToRec = (*ipzVPDMap).find("VINI");
            if (itrToRec == (*ipzVPDMap).end())
            {
                errCode = error_code::RECORD_NOT_FOUND;
                return rc;
            }

            std::string ccinFromVpd{
                vpdSpecificUtility::getKwVal(itrToRec->second, "CC", errCode)};
            if (ccinFromVpd.empty())
            {
                errCode = error_code::KEYWORD_NOT_FOUND;
                return rc;
            }

            transform(ccinFromVpd.begin(), ccinFromVpd.end(),
                      ccinFromVpd.begin(), ::toupper);

            for (std::string ccinValue : jsonObject["ccin"])
            {
                transform(ccinValue.begin(), ccinValue.end(), ccinValue.begin(),
                          ::toupper);

                if (ccinValue.compare(ccinFromVpd) ==
                    constants::STR_CMP_SUCCESS)
                {
                    // CCIN found
                    rc = true;
                }
            }

            if (!rc)
            {
                Logger::getLoggerInstance()->logMessage(
                    "No match found for CCIN");
            }
        }
        else
        {
            errCode = error_code::UNSUPPORTED_VPD_TYPE;
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }
    return rc;
}

/**
 * @brief API to reset data of a FRU populated under PIM.
 *
 * This API resets the data for particular interfaces of a FRU under PIM.
 *
 * @param[in] objectPath - DBus object path of the FRU.
 * @param[in] interfaceMap - Interface and its properties map.
 * @param[in] clearPresence - Indicates whether to clear present property or
 * not.
 * @param[out] errCode - To set error code in case of error.
 */
inline void resetDataUnderPIM(const std::string& objectPath,
                              types::InterfaceMap& interfaceMap,
                              bool clearPresence, uint16_t& errCode)
{
    errCode = 0;
    if (objectPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return;
    }

    try
    {
        std::vector<std::string> interfaces;
        const types::MapperGetObject& getObjectMap =
            dbusUtility::getObjectMap(objectPath, interfaces);

        static const std::vector<std::string> vpdRelatedInterfaces{
            constants::operationalStatusInf, constants::inventoryItemInf,
            constants::assetInf, constants::vpdCollectionInterface};

        for (const auto& [service, interfaceList] : getObjectMap)
        {
            if (service.compare(constants::pimServiceName) !=
                constants::STR_CMP_SUCCESS)
            {
                continue;
            }

            for (const auto& interface : interfaceList)
            {
                if ((interface.find(constants::ipzVpdInf) !=
                         std::string::npos &&
                     interface != constants::locationCodeInf) ||
                    ((std::find(vpdRelatedInterfaces.begin(),
                                vpdRelatedInterfaces.end(), interface)) !=
                     vpdRelatedInterfaces.end()))
                {
                    const types::PropertyMap& propertyValueMap =
                        dbusUtility::getPropertyMap(service, objectPath,
                                                    interface);

                    types::PropertyMap propMap;

                    for (const auto& aProperty : propertyValueMap)
                    {
                        const std::string& propertyName = aProperty.first;
                        const auto& propertyValue = aProperty.second;

                        if (std::holds_alternative<types::BinaryVector>(
                                propertyValue))
                        {
                            propMap.emplace(propertyName,
                                            types::BinaryVector{});
                        }
                        else if (std::holds_alternative<std::string>(
                                     propertyValue))
                        {
                            if (propertyName.compare("Status") ==
                                constants::STR_CMP_SUCCESS)
                            {
                                propMap.emplace(
                                    propertyName,
                                    constants::vpdCollectionNotStarted);
                                propMap.emplace("StartTime", 0);
                                propMap.emplace("CompletedTime", 0);
                            }
                            else if (propertyName.compare("PrettyName") ==
                                     constants::STR_CMP_SUCCESS)
                            {
                                // The FRU name is constant and independent of
                                // its presence state. So, it should not get
                                // reset.
                                continue;
                            }
                            else
                            {
                                propMap.emplace(propertyName, std::string{});
                            }
                        }
                        else if (std::holds_alternative<bool>(propertyValue))
                        {
                            if (propertyName.compare("Present") ==
                                constants::STR_CMP_SUCCESS)
                            {
                                if (clearPresence)
                                {
                                    propMap.emplace(propertyName, false);
                                }
                            }
                            else if (propertyName.compare("Functional") ==
                                     constants::STR_CMP_SUCCESS)
                            {
                                // Since FRU is not present functional property
                                // is considered as true.
                                propMap.emplace(propertyName, true);
                            }
                        }
                    }
                    interfaceMap.emplace(interface, std::move(propMap));
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }
}

/**
 * @brief API to detect pass1 planar type.
 *
 * Based on HW version and IM keyword, This API detects is it is a pass1 planar
 * or not.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return True if pass 1 planar, false otherwise.
 */
inline bool isPass1Planar(uint16_t& errCode) noexcept
{
    errCode = 0;
    bool rc{false};
    const auto hwVar = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::viniInf, constants::kwdHW);

    auto hwVer = std::get_if<types::BinaryVector>(&hwVar);
    if (hwVer == nullptr)
    {
        errCode = error_code::INVALID_VALUE_READ_FROM_DBUS;
        return rc;
    }

    const auto imVar = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsbpInf, constants::kwdIM);

    auto imValue = std::get_if<types::BinaryVector>(&imVar);
    if (imValue == nullptr)
    {
        errCode = error_code::INVALID_VALUE_READ_FROM_DBUS;
        return rc;
    }

    if (hwVer->size() != constants::VALUE_2)
    {
        errCode = error_code::INVALID_KEYWORD_LENGTH;
        return rc;
    }

    if (imValue->size() != constants::VALUE_4)
    {
        errCode = error_code::INVALID_KEYWORD_LENGTH;
        return rc;
    }

    const types::BinaryVector everest{80, 00, 48, 00};
    const types::BinaryVector fuji{96, 00, 32, 00};

    if (((*imValue) == everest) || ((*imValue) == fuji))
    {
        if ((*hwVer).at(1) < constants::VALUE_21)
        {
            rc = true;
        }
    }
    else if ((*hwVer).at(1) < constants::VALUE_2)
    {
        rc = true;
    }

    return rc;
}

/**
 * @brief API to get interface(s) properties corresponding to given
 * record and keyword.
 *
 * For a given record and keyword, this API finds the corresponding
 * interfaces(s) properties from the system config JSON and populates an
 * interface map with the respective properties and values.
 *
 * @param[in] paramsToWriteData - Input details.
 * @param[in] interfaceJson - Interface JSON object.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return Returns a map of interface(s) and properties corresponding to
 * the record and keyword. An empty map is returned if no such
 * interface(s) and properties are found.
 */
inline types::InterfaceMap getInterfaceProperties(
    const types::WriteVpdParams& paramsToWriteData,
    const nlohmann::json& interfaceJson, uint16_t& errCode) noexcept
{
    types::InterfaceMap interfaceMap;
    errCode = 0;
    try
    {
        if (interfaceJson.empty())
        {
            errCode = error_code::INVALID_INPUT_PARAMETER;
            return interfaceMap;
        }

        const types::IpzData* ipzData =
            std::get_if<types::IpzData>(&paramsToWriteData);

        if (!ipzData)
        {
            errCode = error_code::UNSUPPORTED_VPD_TYPE;
            return interfaceMap;
        }

        auto populateInterfaceMap = [&ipzData = std::as_const(ipzData),
                                     &interfaceMap,
                                     &errCode](const auto& interfacesPropPair) {
            if (interfacesPropPair.value().empty())
            {
                return;
            }

            // find matching property value pair
            const auto matchPropValuePairIt = std::find_if(
                interfacesPropPair.value().items().begin(),
                interfacesPropPair.value().items().end(),
                [&ipzData](const auto& propValuePair) {
                    return (propValuePair.value().value("recordName", "") ==
                                std::get<0>(*ipzData) &&
                            propValuePair.value().value("keywordName", "") ==
                                std::get<1>(*ipzData));
                });

            if (matchPropValuePairIt !=
                interfacesPropPair.value().items().end())
            {
                std::string kwd = std::string(std::get<2>(*ipzData).begin(),
                                              std::get<2>(*ipzData).end());

                std::string encodedValue = vpdSpecificUtility::encodeKeyword(
                    kwd, matchPropValuePairIt.value().value("encoding", ""),
                    errCode);

                if (encodedValue.empty() && errCode)
                {
                    Logger::getLoggerInstance()->logMessage(
                        "Failed to get encoded value for keyword : " + kwd +
                        ", error : " + commonUtility::getErrCodeMsg(errCode));
                }

                // add property map to interface map
                interfaceMap.emplace(
                    interfacesPropPair.key(),
                    types::PropertyMap{
                        {matchPropValuePairIt.key(), encodedValue}});
            }
        };

        if (!interfaceJson.empty())
        {
            // iterate through all interfaces and populate interface map
            std::for_each(interfaceJson.items().begin(),
                          interfaceJson.items().end(), populateInterfaceMap);
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }
    return interfaceMap;
}

/**
 * @brief API to update common interface(s) properties when keyword is updated.
 *
 * For a given keyword update on a EEPROM path, this API syncs the keyword
 * update to respective common interface(s) properties of the base FRU and all
 * inherited FRUs.
 *
 * @param[in] fruPath - EEPROM path of FRU.
 * @param[in] paramsToWriteData - Input details.
 * @param[in] sysCfgJsonObj - System config JSON.
 * @param[out] errCode - To set error code in case of error.
 */
inline void updateCiPropertyOfInheritedFrus(
    const std::string& fruPath, const types::WriteVpdParams& paramsToWriteData,
    const nlohmann::json& sysCfgJsonObj, uint16_t& errCode) noexcept
{
    errCode = 0;
    try
    {
        if (fruPath.empty() || sysCfgJsonObj.empty())
        {
            errCode = error_code::INVALID_INPUT_PARAMETER;
            return;
        }

        if (!sysCfgJsonObj.contains("commonInterfaces"))
        {
            // no common interfaces in JSON, nothing to do
            return;
        }

        if (!sysCfgJsonObj.contains("frus"))
        {
            errCode = error_code::INVALID_JSON;
            return;
        }

        if (!sysCfgJsonObj["frus"].contains(fruPath))
        {
            errCode = error_code::FRU_PATH_NOT_FOUND;
            return;
        }

        if (!std::get_if<types::IpzData>(&paramsToWriteData))
        {
            errCode = error_code::UNSUPPORTED_VPD_TYPE;
            return;
        }

        /**
         * Update common interface properties if any of the following conditions
         * is met:
         *
         * 1. "inherit" is set to true for the inventory item, indicating that
         * all records are inherited from the base FRU. In this case, the
         * common interface properties must also be updated.
         * 2. "inheritCI" is set to true for the inventory item, indicating that
         * the item inherits selected records from the base FRU. In this
         * case, the common interface properties must also be updated.
         */

        types::ObjectMap objectInterfaceMap;

        const types::InterfaceMap ifaceMap = getInterfaceProperties(
            paramsToWriteData, sysCfgJsonObj["commonInterfaces"], errCode);

        if (ifaceMap.empty())
        {
            if (errCode)
            {
                Logger::getLoggerInstance()->logMessage(
                    "Failed to get common interface property list, error : " +
                    commonUtility::getErrCodeMsg(errCode));
            }
            // nothing to do
            return;
        }

        // update common interfaces if either inherit or inheritCI value is
        // true.
        auto populateObjectInterfaceMap = [&objectInterfaceMap,
                                           &ifaceMap = std::as_const(ifaceMap)](
                                              const auto& fru) {
            if ((fru.value("inherit", true) || fru.value("inheritCI", false)) &&
                fru.contains("inventoryPath"))
            {
                objectInterfaceMap.emplace(
                    sdbusplus::object_path{fru["inventoryPath"]}, ifaceMap);
            }
        };

        std::for_each(sysCfgJsonObj["frus"][fruPath].begin(),
                      sysCfgJsonObj["frus"][fruPath].end(),
                      populateObjectInterfaceMap);

        if (!objectInterfaceMap.empty())
        {
            // Call method to update the dbus
            if (!dbusUtility::publishVpdOnDBus(move(objectInterfaceMap)))
            {
                errCode = error_code::DBUS_FAILURE;
                return;
            }
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }
}

/**
 * @brief API to convert write VPD parameters to a string.
 *
 * @param[in] paramsToWriteData - write VPD parameters.
 * @param[out] errCode - To set error code in case of error.
 *
 * @return On success returns string representation of write VPD parameters,
 * otherwise returns an empty string.
 */
inline const std::string convertWriteVpdParamsToString(
    const types::WriteVpdParams& paramsToWriteData, uint16_t& errCode) noexcept
{
    errCode = 0;
    try
    {
        if (const types::IpzData* ipzDataPtr =
                std::get_if<types::IpzData>(&paramsToWriteData))
        {
            return std::string{
                "Record: " + std::get<0>(*ipzDataPtr) +
                " Keyword: " + std::get<1>(*ipzDataPtr) + " Value: " +
                commonUtility::convertByteVectorToHex(
                    std::get<2>(*ipzDataPtr))};
        }
        else if (const types::KwData* kwDataPtr =
                     std::get_if<types::KwData>(&paramsToWriteData))
        {
            return std::string{
                "Keyword: " + std::get<0>(*kwDataPtr) + " Value: " +
                commonUtility::convertByteVectorToHex(std::get<1>(*kwDataPtr))};
        }
        else
        {
            errCode = error_code::UNSUPPORTED_VPD_TYPE;
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;
    }
    return std::string{};
}

/**
 * @brief An API to read IM value from VPD.
 *
 * @param[in] parsedVpd - Parsed VPD.
 * @param[out] errCode - To set error code in case of error.
 */
inline std::string getIMValue(const types::IPZVpdMap& parsedVpd,
                              uint16_t& errCode) noexcept
{
    errCode = 0;
    std::ostringstream imData;
    try
    {
        if (parsedVpd.empty())
        {
            errCode = error_code::INVALID_INPUT_PARAMETER;
            return {};
        }

        const auto& itrToVSBP = parsedVpd.find("VSBP");
        if (itrToVSBP == parsedVpd.end())
        {
            errCode = error_code::RECORD_NOT_FOUND;
            return {};
        }

        const auto& itrToIM = (itrToVSBP->second).find("IM");
        if (itrToIM == (itrToVSBP->second).end())
        {
            errCode = error_code::KEYWORD_NOT_FOUND;
            return {};
        }

        types::BinaryVector imVal;
        std::copy(itrToIM->second.begin(), itrToIM->second.end(),
                  back_inserter(imVal));

        for (auto& aByte : imVal)
        {
            imData << std::setw(2) << std::setfill('0') << std::hex
                   << static_cast<int>(aByte);
        }
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to get IM value with exception:" + std::string(ex.what()));
        errCode = error_code::STANDARD_EXCEPTION;
    }

    return imData.str();
}

/**
 * @brief An API to read HW version from VPD.
 *
 * @param[in] parsedVpd - Parsed VPD.
 * @param[out] errCode - To set error code in case of error.
 */
inline std::string getHWVersion(const types::IPZVpdMap& parsedVpd,
                                uint16_t& errCode) noexcept
{
    errCode = 0;
    std::ostringstream hwString;
    try
    {
        if (parsedVpd.empty())
        {
            errCode = error_code::INVALID_INPUT_PARAMETER;
            return {};
        }

        const auto& itrToVINI = parsedVpd.find("VINI");
        if (itrToVINI == parsedVpd.end())
        {
            errCode = error_code::RECORD_NOT_FOUND;
            return {};
        }

        const auto& itrToHW = (itrToVINI->second).find("HW");
        if (itrToHW == (itrToVINI->second).end())
        {
            errCode = error_code::KEYWORD_NOT_FOUND;
            return {};
        }

        types::BinaryVector hwVal;
        std::copy(itrToHW->second.begin(), itrToHW->second.end(),
                  back_inserter(hwVal));

        // The planar pass only comes from the LSB of the HW keyword,
        // where as the MSB is used for other purposes such as signifying clock
        // termination.
        hwVal[0] = 0x00;

        for (auto& aByte : hwVal)
        {
            hwString << std::setw(2) << std::setfill('0') << std::hex
                     << static_cast<int>(aByte);
        }
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to get HW version with exception:" +
            std::string(ex.what()));
        errCode = error_code::STANDARD_EXCEPTION;
    }

    return hwString.str();
}

/**
 * @brief An API to set VPD collection status for a fru.
 *
 * This API updates the CollectionStatus property of the given FRU with the
 * given value.
 *
 * @param[in] vpdPath - Fru path (EEPROM or Inventory path)
 * @param[in] value - State to set.
 * @param[in] sysCfgJsonObj - System config json object.
 * @param[out] errCode - To set error code in case of error.
 */
inline void setCollectionStatusProperty(
    const std::string& vpdPath, const types::VpdCollectionStatus& value,
    const nlohmann::json& sysCfgJsonObj, uint16_t& errCode) noexcept
{
    errCode = 0;
    if (vpdPath.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return;
    }

    if (sysCfgJsonObj.empty() || !sysCfgJsonObj.contains("frus"))
    {
        errCode = error_code::INVALID_JSON;
        return;
    }

    types::PropertyMap timeStampMap;
    if (value == types::VpdCollectionStatus::Completed ||
        value == types::VpdCollectionStatus::Failed)
    {
        timeStampMap.emplace(
            "CompletedTime",
            types::DbusVariantType{commonUtility::getCurrentTimeSinceEpoch()});
    }
    else if (value == types::VpdCollectionStatus::InProgress)
    {
        timeStampMap.emplace(
            "StartTime",
            types::DbusVariantType{commonUtility::getCurrentTimeSinceEpoch()});
    }
    else if (value == types::VpdCollectionStatus::NotStarted)
    {
        timeStampMap.emplace("StartTime", 0);
        timeStampMap.emplace("CompletedTime", 0);
    }

    types::ObjectMap objectInterfaceMap;

    const auto& eepromPath = jsonUtility::getFruPathFromJson(vpdPath, errCode);

    if (eepromPath.empty() || errCode)
    {
        return;
    }

    for (const auto& fru : sysCfgJsonObj["frus"][eepromPath])
    {
        sdbusplus::object_path fruObjectPath(fru["inventoryPath"]);

        types::PropertyMap propertyValueMap;
        propertyValueMap.emplace(
            "Status",
            types::CommonProgress::convertOperationStatusToString(value));
        propertyValueMap.insert(timeStampMap.begin(), timeStampMap.end());

        types::InterfaceMap interfaces;
        vpdSpecificUtility::insertOrMerge(interfaces,
                                          types::CommonProgress::interface,
                                          move(propertyValueMap), errCode);

        if (errCode)
        {
            Logger::getLoggerInstance()->logMessage(
                "Failed to insert value into map, error : " +
                commonUtility::getErrCodeMsg(errCode));
            return;
        }

        objectInterfaceMap.emplace(std::move(fruObjectPath),
                                   std::move(interfaces));
    }

    // Call dbus method to update on dbus
    if (!dbusUtility::publishVpdOnDBus(move(objectInterfaceMap)))
    {
        errCode = error_code::DBUS_FAILURE;
        return;
    }
}

/**
 * @brief API to reset data of a FRU and its sub-FRU populated under PIM.
 *
 * The API resets the data for specific interfaces of a FRU and its sub-FRUs
 * under PIM.
 *
 * Note: vpdPath should be either the base inventory path or the EEPROM path.
 *
 * @param[in] vpdPath - EEPROM/root inventory path of the FRU.
 * @param[in] sysCfgJsonObj - system config JSON.
 * @param[out] errCode - To set error code in case of error.
 */
inline void resetObjTreeVpd(const std::string& vpdPath,
                            const nlohmann::json& sysCfgJsonObj,
                            uint16_t& errCode) noexcept
{
    errCode = 0;
    if (vpdPath.empty() || sysCfgJsonObj.empty())
    {
        errCode = error_code::INVALID_INPUT_PARAMETER;
        return;
    }

    try
    {
        const std::string& fruPath =
            jsonUtility::getFruPathFromJson(vpdPath, errCode);

        if (errCode)
        {
            return;
        }

        types::ObjectMap objectMap;

        const auto& fruItems = sysCfgJsonObj["frus"][fruPath];

        for (const auto& inventoryItem : fruItems)
        {
            const std::string& objPath =
                inventoryItem.value("inventoryPath", "");

            if (inventoryItem.value("synthesized", false))
            {
                continue;
            }

            types::InterfaceMap ifaceMap;
            resetDataUnderPIM(objPath, ifaceMap,
                              inventoryItem.value("handlePresence", true),
                              errCode);

            if (errCode)
            {
                Logger::getLoggerInstance()->logMessage(
                    "Failed to get data to clear on DBus for path [" + objPath +
                    "], error : " + commonUtility::getErrCodeMsg(errCode));

                continue;
            }

            objectMap.emplace(objPath, ifaceMap);
        }

        if (!dbusUtility::publishVpdOnDBus(std::move(objectMap)))
        {
            errCode = error_code::DBUS_FAILURE;
        }
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to reset FRU data on DBus for FRU [" + vpdPath +
            "], error : " + std::string(ex.what()));

        errCode = error_code::STANDARD_EXCEPTION;
    }
}

/**
 * @brief Converts a record-to-keywords map into a human-readable string.
 *
 * This API formats the contents of the provided RecordKeywordsMap
 * into a printable string representation. Each record and its associated
 * keywords are appended in a structured format.
 *
 * @param[in] recordKeywordMap  Map of record names to their keyword list.
 *
 * @return A formatted string containing the map contents, or an empty
 *         string if the input map is empty.
 *
 * @note The output string is generated in the following format:
 *       [{record1:[kw1, kw2]}, {record2:[kw3, kw4]}, ...]
 *       Example:  [{VSYS:[BR, J0]}]
 */
inline std::string getInStringFormat(
    const types::RecordKeywordsMap& recordKeywordMap) noexcept
{
    if (recordKeywordMap.empty())
    {
        return std::string{};
    }

    std::ostringstream message;
    message << "[";
    bool firstHandled = false;

    for (const auto& recordKws : recordKeywordMap)
    {
        if (firstHandled)
        {
            message << ", ";
        }

        const auto keywords = recordKws.second |
                              std::views::join_with(std::string(", "));

        message << "{" + recordKws.first + ":[" +
                       std::string(keywords.begin(), keywords.end()) + "]}";
        firstHandled = true;
    }

    message << "]. ";
    return message.str();
}

/**
 * @brief Extract chassis ID from given inventory object path.
 *
 * @param[in] inventoryObjPath - Inventory object path.
 * @param[out] errCode - Error code, 0 on success.
 *
 * @return Chassis ID on successful extraction, empty string otherwise.
 */
inline std::string getChassisId(const std::string& inventoryObjPath,
                                uint16_t& errCode) noexcept
{
    try
    {
        auto startPos = inventoryObjPath.find("/chassis");
        if (std::string::npos == startPos)
        {
            return std::string{};
        }

        ++startPos;
        const auto endPos = inventoryObjPath.find('/', startPos);
        const auto chassisId =
            inventoryObjPath.substr(startPos, endPos - startPos);

        return chassisId;
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to extract chassis ID from given path {}, error: {}",
            inventoryObjPath, ex.what()));
        errCode = error_code::STANDARD_EXCEPTION;
        return std::string{};
    }
}

/**
 * @brief Builds expanded location code from unexpanded location code.
 *
 * @param[in] unexpandedLocationCode - Unexpanded location code.
 * @param[in] isFcs - True if FCS location code, false if MTS.
 * @param[in] pos - Position where "fcs" or "mts" prefix was found.
 * @param[in] firstKwdValue - First keyword value (FC for FCS, TM for MTS).
 * @param[in] secondKwdValue - Second keyword value (SE).
 * @param[in] nodeIdentifier - Node identifier to embed in the expanded LC.
 *                               For FCS: "N00", "N01", etc. (derived from
 *                               chassis ID). Empty for MTS.
 *
 * @return Expanded location code string. In case of error, unexpanded is
 *         returned.
 */
inline std::string buildExpandedLc(
    const std::string& unexpandedLocationCode, bool isFcs, size_t pos,
    const std::string& firstKwdValue, const std::string& secondKwdValue,
    const std::string& nodeIdentifier) noexcept
{
    try
    {
        std::string expandedLC = unexpandedLocationCode;
        if (isFcs)
        {
            const std::string suffix = unexpandedLocationCode.substr(
                pos + constants::LOCATION_CODE_PREFIX_LENGTH);

            if (suffix.empty())
            {
                //"fcs"/"mts" is replaced with 4 bytes of VCEN:FC value.
                expandedLC.replace(
                    pos, constants::LOCATION_CODE_PREFIX_LENGTH,
                    firstKwdValue.substr(constants::FIRST_POSITION,
                                         constants::FC_KEYWORD_FIRST_4_BYTE) +
                        "." + nodeIdentifier + "." + secondKwdValue);
            }
            else if (suffix[constants::FIRST_POSITION] == '-')
            {
                expandedLC.replace(
                    pos, constants::LOCATION_CODE_PREFIX_LENGTH,
                    firstKwdValue.substr(constants::FIRST_POSITION,
                                         constants::FC_KEYWORD_FIRST_4_BYTE) +
                        "." + nodeIdentifier + "." + secondKwdValue);
            }
        }
        else
        {
            // MTS: replace dashes in TM value with dots.
            std::string firstKwdValueCopy = firstKwdValue;
            std::replace(firstKwdValueCopy.begin(), firstKwdValueCopy.end(),
                         '-', '.');
            expandedLC.replace(pos, constants::LOCATION_CODE_PREFIX_LENGTH,
                               firstKwdValueCopy + "." + secondKwdValue);
        }

        return expandedLC;
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "buildExpandedLc failed for unexpandedLocationCode: {}. Reason: {}",
            unexpandedLocationCode, ex.what()));

        return unexpandedLocationCode;
    }
}

/**
 * @brief Derives the node identifier string from a chassis ID.
 *
 * Maps the chassis segment of an inventory object path to the node identifier
 * string embedded in expanded location codes:
 *   "chassis"  (legacy, no numeric suffix) -> "N00"
 *   "chassis0"                             -> "SC0"
 *   "chassis1"                             -> "N00"
 *   "chassis2"                             -> "N01"
 *   "chassisN"                             -> "N{N-1}" (zero-padded to 2
 * digits)
 *
 * @param[in] chassisId - Chassis ID string (e.g. "chassis", "chassis0",
 *                          "chassis2").
 *
 * @return On success, returns the node identifier string. On failure, returns
 *         an error code
 */
inline std::expected<std::string, error_code> getNodeIdentifierFromChassisId(
    const std::string& chassisId) noexcept
{
    try
    {
        constexpr std::string_view prefix{"chassis"};

        if (!chassisId.starts_with(prefix))
        {
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        const std::string nodeNumber = chassisId.substr(prefix.size());

        if (nodeNumber.empty())
        {
            // Legacy path with no numeric suffix (e.g. ".../chassis/...").
            return "N00";
        }

        // Reject malformed suffixes like "chassis_bad" or "chassis1abc".
        if (!std::all_of(nodeNumber.begin(), nodeNumber.end(), ::isdigit))
        {
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        if (nodeNumber == "0")
        {
            return "SC0";
        }

        const size_t num = std::stoul(nodeNumber);
        return std::format("N{:02}", num - 1);
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to get node identifier from chassis ID: {}. Error: {}",
            chassisId, ex.what()));
        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
}

/**
 * @brief Builds a node-qualified unexpanded location code from an unexpanded
 * location code and a node number.
 *
 * Derives the node identifier string from @p nodeNumber. Node number 0
 * maps to "SC0", 1 maps to "N00", 2 maps to "N01", and so on. Inserts the
 * node identifier into @p unexpandedLocationCode right after the first '-'
 * (e.g. "Ufcs-P0" -> "Ufcs-N00-P0"). If no '-' is present (bare prefix like
 * "Ufcs"), the node identifier is appended (e.g. "Ufcs" -> "Ufcs-N00").
 *
 * @param[in] unexpandedLocationCode - Unexpanded location code.
 * @param[in] nodeNumber - Node number as received from the D-Bus caller.
 *
 * @return on Success, returns node-qualified unexpanded location code string
 * On failure, returns an error code
 *
 * @note The caller is responsible for ensuring the location code is valid (e.g.
 * starts with "Ufcs" or "Umts" and meets the minimum length requirement) before
 * invoking this function. Use isValidUnexpandedLocationCode() to validate
 * beforehand.
 */
inline std::expected<std::string, error_code> buildNodeQualifiedLocCode(
    const std::string& unexpandedLocationCode,
    const uint16_t nodeNumber) noexcept
{
    try
    {
        const std::string nodeId =
            (nodeNumber == constants::VALUE_0)
                ? "SC0"
                : std::format("N{:02}", static_cast<size_t>(nodeNumber - 1));

        const auto dashPos = unexpandedLocationCode.find('-');
        if (dashPos != std::string::npos)
        {
            return unexpandedLocationCode.substr(0, dashPos + 1) + nodeId +
                   "-" + unexpandedLocationCode.substr(dashPos + 1);
        }

        return unexpandedLocationCode + "-" + nodeId;
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to build node qualified location code for {} with node number {}. Error: {}",
            unexpandedLocationCode, nodeNumber, ex.what()));
        return std::unexpected(error_code::STANDARD_EXCEPTION);
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
 * @param[in] inventoryPath - Inventory Path.
 * @param[in] unexpandedLocationCode - Unexpanded location code.
 * @param[in] parsedVpdMap - Parsed VPD map.
 * @param[in] pos - Position of "fcs" in the unexpanded location code.
 *
 * @return std::expected with expanded location code on success, or error code
 *         on failure.
 * @throw Any exception is propagated to the caller.
 */
inline std::expected<std::string, uint16_t> getFcsExpandedLc(
    const std::string& inventoryPath, const std::string& unexpandedLocationCode,
    const types::VPDMapVariant& parsedVpdMap, size_t pos)
{
    if (inventoryPath.empty())
    {
        return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
    }

    uint16_t errCode{0};

    const auto chassisId = getChassisId(inventoryPath, errCode);
    if (errCode)
    {
        return std::unexpected(errCode);
    }

    if (chassisId.empty())
    {
        // Implies "/chassis" not found in inventory path.
        return std::unexpected(INVALID_INVENTORY_PATH);
    }

    const std::string chassisInvPath =
        std::format("{}/{}", constants::systemVpdInvPath, chassisId);

    const auto mapperRetValue =
        dbusUtility::getObjectMap(chassisInvPath, {constants::vcenInf});

    std::string fcKwdValue;
    std::string seKwdValue;

    if (!mapperRetValue.empty())
    {
        // Object mapper returned a result — read FC and SE from D-Bus.
        const std::string& service = mapperRetValue.begin()->first;

        const auto readKwd = [&](const std::string& kwd) -> std::string {
            const auto retVal = dbusUtility::readDbusProperty(
                service, chassisInvPath, constants::vcenInf, kwd);

            if (const auto val = std::get_if<types::BinaryVector>(&retVal))
            {
                return std::string(reinterpret_cast<const char*>(val->data()),
                                   val->size());
            }

            errCode = error_code::RECEIVED_INVALID_KWD_TYPE_FROM_DBUS;
            Logger::getLoggerInstance()->logMessage(
                std::format("Failed to read kwd {} from Dbus", kwd));

            return {};
        };

        fcKwdValue = readKwd(constants::kwdFC);
        if (errCode)
        {
            return std::unexpected(errCode);
        }

        seKwdValue = readKwd(constants::kwdSE);
        if (errCode)
        {
            return std::unexpected(errCode);
        }

        if (fcKwdValue.empty() || seKwdValue.empty())
        {
            return std::unexpected(error_code::INVALID_VALUE_READ_FROM_DBUS);
        }

        // FC keyword must be at least 4 characters for a valid substr.
        if (fcKwdValue.size() < constants::FC_KEYWORD_FIRST_4_BYTE)
        {
            Logger::getLoggerInstance()->logMessage(std::format(
                "FC keyword value '{}' read from Dbus, is too short (expected at least 4 characters)",
                fcKwdValue));

            return std::unexpected(error_code::INVALID_KEYWORD_LENGTH);
        }
    }
    else
    {
        // Object mapper returned empty — fall back to VPD map (VCEN record).
        const auto ipzMap = std::get_if<types::IPZVpdMap>(&parsedVpdMap);
        if (!ipzMap)
        {
            return std::unexpected(error_code::UNSUPPORTED_VPD_TYPE);
        }

        const auto vcenItr = ipzMap->find(constants::recVCEN);
        if (vcenItr == ipzMap->end())
        {
            return std::unexpected(error_code::RECORD_NOT_FOUND);
        }

        fcKwdValue = getKwVal(vcenItr->second, constants::kwdFC, errCode);
        if (errCode)
        {
            return std::unexpected(errCode);
        }

        seKwdValue = getKwVal(vcenItr->second, constants::kwdSE, errCode);
        if (errCode)
        {
            return std::unexpected(errCode);
        }

        if (fcKwdValue.empty() || seKwdValue.empty())
        {
            return std::unexpected(error_code::INVALID_VALUE_READ_FROM_EEPROM);
        }

        // FC keyword must be at least 4 characters for a valid substr.
        if (fcKwdValue.size() < constants::FC_KEYWORD_FIRST_4_BYTE)
        {
            Logger::getLoggerInstance()->logMessage(std::format(
                "FC keyword value '{}' read from EEPROM, is too short (expected at least 4 characters)",
                fcKwdValue));

            return std::unexpected(error_code::INVALID_KEYWORD_LENGTH);
        }
    }
    const auto nodeIdentifierResult =
        vpdSpecificUtility::getNodeIdentifierFromChassisId(chassisId);

    if (!nodeIdentifierResult.has_value())
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Invalid node value read from inventory path: {}. "
            "Error: {}",
            inventoryPath,
            commonUtility::getErrCodeMsg(nodeIdentifierResult.error())));
        return std::unexpected(nodeIdentifierResult.error());
    }

    return buildExpandedLc(unexpandedLocationCode, true, pos, fcKwdValue,
                           seKwdValue, nodeIdentifierResult.value());
}

/**
 * @brief Expands an MTS-type unexpanded location code.
 *
 * Reads VSYS-TM and VSYS-SE value from the parsed IPZ VPD map and calls
 * buildExpandedLc. Any exception is propagated to the caller.
 *
 * @param[in] unexpandedLocationCode - Unexpanded location code.
 * @param[in] parsedVpdMap - Parsed VPD map.
 * @param[in] pos - Position of "mts" in the unexpanded location code.
 *
 * @return std::expected with expanded location code on success, or error code
 *         on failure.
 *
 * @throw exception in case of failure. Caller needs to handle.
 */
inline std::expected<std::string, uint16_t> getMtsExpandedLc(
    const std::string& unexpandedLocationCode,
    const types::VPDMapVariant& parsedVpdMap, size_t pos)
{
    uint16_t errCode{0};

    const auto ipzMap = std::get_if<types::IPZVpdMap>(&parsedVpdMap);
    if (!ipzMap)
    {
        return std::unexpected(error_code::UNSUPPORTED_VPD_TYPE);
    }

    const auto recItr = ipzMap->find(constants::recVSYS);
    if (recItr == ipzMap->end())
    {
        return std::unexpected(error_code::RECORD_NOT_FOUND);
    }

    const std::string tmKwdValue =
        getKwVal(recItr->second, constants::kwdTM, errCode);
    if (errCode)
    {
        return std::unexpected(errCode);
    }

    const std::string seKwdValue =
        getKwVal(recItr->second, constants::kwdSE, errCode);
    if (errCode)
    {
        return std::unexpected(errCode);
    }

    if (tmKwdValue.empty() || seKwdValue.empty())
    {
        return std::unexpected(error_code::INVALID_VALUE_READ_FROM_EEPROM);
    }

    return buildExpandedLc(unexpandedLocationCode, false, pos, tmKwdValue,
                           seKwdValue, {});
}

/**
 * @brief API to expand unexpanded location code.
 *
 * Detects whether the location code is FCS or MTS type and delegates
 * expansion to getFcsExpandedLc or getMtsExpandedLc respectively.
 *
 * @param[in] inventoryPath - Inventory Path.
 * @param[in] unexpandedLocationCode - Unexpanded location code.
 * @param[in] parsedVpdMap - Parsed VPD map.
 *
 * @return std::expected with expanded location code on success, or error code
 *         on failure.
 */
inline std::expected<std::string, uint16_t> getExpandedLocationCode(
    const std::string& inventoryPath, const std::string& unexpandedLocationCode,
    const types::VPDMapVariant& parsedVpdMap) noexcept
{
    if (unexpandedLocationCode.empty())
    {
        return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
    }

    try
    {
        size_t pos = unexpandedLocationCode.find(constants::fcsTypeLc);
        if (pos != std::string::npos)
        {
            return getFcsExpandedLc(inventoryPath, unexpandedLocationCode,
                                    parsedVpdMap, pos);
        }

        pos = unexpandedLocationCode.find(constants::mtsTypeLc);
        if (pos != std::string::npos)
        {
            return getMtsExpandedLc(unexpandedLocationCode, parsedVpdMap, pos);
        }

        return std::unexpected(error_code::FAILED_TO_DETECT_LOCATION_CODE_TYPE);
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "getExpandedLocationCode failed for FRU: {}, unexpandedLocationCode: {}. Reason: {}",
            inventoryPath, unexpandedLocationCode, ex.what()));

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
 * @param[in] fruPath - EEPROM path of the FRU.
 * @param[in] paramsToWriteData - Write VPD parameters
 * @param[in] sysCfgJsonObj - Config JSON object.
 * @param[out] errCode - To set error code incase of failure.
 */
inline void updateKeywordOnDBus(
    const std::string& fruPath, const types::WriteVpdParams& paramsToWriteData,
    const nlohmann::json& sysCfgJsonObj, uint16_t& errCode)
{
    errCode = 0;
    try
    {
        if (fruPath.empty())
        {
            errCode = error_code::INVALID_INPUT_PARAMETER;
            return;
        }

        if (!sysCfgJsonObj.contains("frus"))
        {
            errCode = error_code::INVALID_JSON;
            return;
        }

        if (!sysCfgJsonObj["frus"].contains(fruPath))
        {
            errCode = error_code::FRU_PATH_NOT_FOUND;
            return;
        }

        const types::IpzData* ipzData =
            std::get_if<types::IpzData>(&paramsToWriteData);

        // @todo Add support for other VPD types.
        if (!ipzData)
        {
            errCode = error_code::UNSUPPORTED_VPD_TYPE;
            return;
        }

        //  iterate through all inventory paths for given EEPROM path,
        //  if for an inventory path, "inherit" tag is true OR the record
        //  is part of "copyRecords" tag, update the inventory path's
        //  com.ibm.ipzvpd.<record>, property

        types::ObjectMap objectInterfaceMap;

        auto populateInterfaceMap = [&objectInterfaceMap,
                                     &ipzData = std::as_const(ipzData)](
                                        const auto& inventoryItem) {
            if (!ipzData)
            {
                return;
            }

            // check if record is part of copyRecords list.
            const bool isPartOfCopyRecord =
                inventoryItem.contains("copyRecords") &&
                std::find(inventoryItem["copyRecords"].begin(),
                          inventoryItem["copyRecords"].end(),
                          std::get<0>(*ipzData)) !=
                    inventoryItem["copyRecords"].end();

            // check if record is part of skipRecords list
            const bool isPartOfSkipRecord =
                inventoryItem.contains("skipRecords") &&
                std::find(inventoryItem["skipRecords"].begin(),
                          inventoryItem["skipRecords"].end(),
                          std::get<0>(*ipzData)) !=
                    inventoryItem["skipRecords"].end();

            /** Update the keyword if any of the following conditions is met:
             *
             * 1. "inherit" is set to true for the inventory item, indicating
             * that all records are inherited from the base FRU.
             * 2. The input record is listed in "copyRecords" for the inventory
             * item, indicating that only the specified records are inherited
             * from the base FRU.
             * 3. "skipRecords" is defined for the inventory item and the input
             * record is not listed in "skipRecords", indicating that all
             * records except those in the skip list are inherited from the
             * base FRU.
             */
            if (inventoryItem.value("inherit", true) ||
                (isPartOfCopyRecord || (inventoryItem.contains("skipRecords") &&
                                        !isPartOfSkipRecord)))
            {
                objectInterfaceMap.emplace(
                    sdbusplus::object_path{inventoryItem["inventoryPath"]},
                    types::InterfaceMap{
                        {constants::ipzVpdInf + std::get<0>(*ipzData),
                         types::PropertyMap{
                             {std::get<1>(*ipzData), std::get<2>(*ipzData)}}}});
            }
        };

        // iterate through all inventory paths of the FRU.
        std::for_each(sysCfgJsonObj["frus"][fruPath].begin(),
                      sysCfgJsonObj["frus"][fruPath].end(),
                      populateInterfaceMap);

        if (!objectInterfaceMap.empty())
        {
            if (!dbusUtility::publishVpdOnDBus(std::move(objectInterfaceMap)))
            {
                errCode = error_code::DBUS_FAILURE;
            }
        }
    }
    catch (const std::exception& ex)
    {
        errCode = error_code::STANDARD_EXCEPTION;

        std::string recordKeywordInfo;
        const types::IpzData* ipzData =
            std::get_if<types::IpzData>(&paramsToWriteData);

        // @todo Add support for other VPD types.
        if (ipzData)
        {
            recordKeywordInfo = std::format(
                "[{}] : [{}]", std::get<0>(*ipzData), std::get<1>(*ipzData));
        }

        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to update keyword [{}] on DBus for path [{}], error : {}.",
            recordKeywordInfo, fruPath, ex.what()));
    }
}

/**
 * @brief API to update extra interface(s) properties when keyword is updated.
 *
 * For a given keyword update on a EEPROM path, this API syncs the keyword
 * update to respective extra interface(s) properties of the base FRU and all
 * sub FRUs.
 *
 * @param[in] fruPath - EEPROM path of FRU.
 * @param[in] paramsToWriteData - Input details.
 * @param[in] sysCfgJsonObj - Config JSON.
 *
 * @return Returns an empty expected object on success, otherwise
 * returns the corresponding error code
 */
inline std::expected<void, error_code> updateExtraInterfaceProperties(
    const std::string& fruPath, const types::WriteVpdParams& paramsToWriteData,
    const nlohmann::json& sysCfgJsonObj) noexcept
{
    const std::vector<std::string> listofInterfacesToUpdate{
        constants::assetInf};
    try
    {
        if (fruPath.empty() || sysCfgJsonObj.empty())
        {
            return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
        }

        if (!sysCfgJsonObj.contains("frus"))
        {
            return std::unexpected(error_code::INVALID_JSON);
        }

        if (!sysCfgJsonObj["frus"].contains(fruPath))
        {
            return std::unexpected(error_code::FRU_PATH_NOT_FOUND);
        }

        if (!std::get_if<types::IpzData>(&paramsToWriteData))
        {
            return std::unexpected(error_code::UNSUPPORTED_VPD_TYPE);
        }

        // Checks whether the inventory item defines any of the interfaces
        // listed in listofInterfacesToUpdate. If so, derives the
        // corresponding property values for the updated keyword and adds them
        // to the object map for publishing the updates on D-Bus.
        types::ObjectMap objectInterfaceMap;
        auto populateObjectInterfaceMap = [&listofInterfacesToUpdate,
                                           &objectInterfaceMap,
                                           &paramsToWriteData](
                                              const auto& inventoryItem) {
            if (!inventoryItem.contains("extraInterfaces"))
            {
                return;
            }

            const auto& extraInterfaces = inventoryItem["extraInterfaces"];
            nlohmann::json interfaceJson{};

            // Get the list of interfaces to be updated
            for (const auto& interface : listofInterfacesToUpdate)
            {
                if (auto iterator = extraInterfaces.find(interface);
                    iterator != extraInterfaces.end())
                {
                    interfaceJson[interface] = iterator.value();
                }
            }

            if (interfaceJson.empty())
            {
                return;
            }

            uint16_t errCode = 0;
            const types::InterfaceMap ifaceMap = getInterfaceProperties(
                paramsToWriteData, interfaceJson, errCode);

            if (errCode)
            {
                Logger::getLoggerInstance()->logMessage(std::format(
                    "Failed to get extra properties interface list for path [{}], error : {}",
                    std::string(inventoryItem["inventoryPath"]),
                    commonUtility::getErrCodeMsg(errCode)));
            }

            if (!ifaceMap.empty())
            {
                objectInterfaceMap.emplace(
                    sdbusplus::object_path{inventoryItem["inventoryPath"]},
                    ifaceMap);
            }
        };

        //  iterate through all inventory paths for given EEPROM path
        //  update the inventory path's corresponding extra interface(s)
        //  property
        std::for_each(sysCfgJsonObj["frus"][fruPath].begin(),
                      sysCfgJsonObj["frus"][fruPath].end(),
                      populateObjectInterfaceMap);

        if (!objectInterfaceMap.empty())
        {
            if (!dbusUtility::publishVpdOnDBus(move(objectInterfaceMap)))
            {
                return std::unexpected(error_code::DBUS_FAILURE);
            }
        }
    }
    catch (const std::exception& ex)
    {
        uint16_t errCode = 0;
        const std::string recordKeywordInfo =
            convertWriteVpdParamsToString(paramsToWriteData, errCode);

        std::string errMsg = ex.what();
        if (errCode)
        {
            errMsg += std::format(
                ". Failed to convert write VPD parameters to string, error {}",
                commonUtility::getErrCodeMsg(errCode));
        }

        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to update the extra interface property corresponding to keyword [{}] on DBus for path [{}], error : {}.",
            recordKeywordInfo, fruPath, errMsg));

        return std::unexpected(error_code::STANDARD_EXCEPTION);
    }
    return {};
}

} // namespace vpdSpecificUtility
} // namespace vpd
