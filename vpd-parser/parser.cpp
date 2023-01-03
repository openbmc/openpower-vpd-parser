#include "parser.hpp"

#include "ibm_vpd_utils.hpp"
#include "parser_factory.hpp"
#include "parser_interface.hpp"
#include "store.hpp"
#include "types.hpp"

#include <iomanip>
#include <iostream>
#include <phosphor-logging/log.hpp>

namespace openpower
{
namespace vpd
{

void Parser::restoreSystemVPD(Parsed& vpdMap, const std::string& objectPath)
{
    for (const auto& systemRecKwdPair : svpdKwdMap)
    {
        auto it = vpdMap.find(systemRecKwdPair.first);

        // check if record is found in map we got by parser
        if (it != vpdMap.end())
        {
            const auto& kwdListForRecord = systemRecKwdPair.second;
            for (const auto& keyword : kwdListForRecord)
            {
                types::DbusPropertyMap& kwdValMap = it->second;
                auto iterator = kwdValMap.find(keyword);

                if (iterator != kwdValMap.end())
                {
                    std::string& kwdValue = iterator->second;

                    // check bus data
                    const std::string& recordName = systemRecKwdPair.first;
                    const std::string& busValue = readBusProperty(
                        objectPath, constants::ipzVpdInf + recordName, keyword);

                    std::string defaultValue{' '};

                    // Explicit check for D0 is required as this keyword will
                    // never be blank and 0x00 should be treated as no value in
                    // this case.
                    if (recordName == "UTIL" && keyword == "D0")
                    {
                        // default value of kwd D0 is 0x00. This kwd will never
                        // be blank.
                        defaultValue = '\0';
                    }

                    if (busValue.find_first_not_of(defaultValue) !=
                        std::string::npos)
                    {
                        if (kwdValue.find_first_not_of(defaultValue) !=
                            std::string::npos)
                        {
                            // both the data are present, check for mismatch
                            if (busValue != kwdValue)
                            {
                                std::string errMsg =
                                    "VPD data mismatch on cache "
                                    "and hardware for record: ";
                                errMsg += (*it).first;
                                errMsg += " and keyword: ";
                                errMsg += keyword;

                                std::ostringstream busStream;
                                for (uint16_t byte : busValue)
                                {
                                    busStream << std::setfill('0')
                                              << std::setw(2) << std::hex
                                              << "0x" << byte << " ";
                                }

                                std::ostringstream vpdStream;
                                for (uint16_t byte : kwdValue)
                                {
                                    vpdStream << std::setfill('0')
                                              << std::setw(2) << std::hex
                                              << "0x" << byte << " ";
                                }

                                // data mismatch
                                types::PelAdditionalData additionalData;
                                additionalData.emplace("CALLOUT_INVENTORY_PATH",
                                                       objectPath);

                                additionalData.emplace("DESCRIPTION", errMsg);
                                additionalData.emplace("Value on Cache: ",
                                                       busStream.str());
                                additionalData.emplace(
                                    "Value read from EEPROM: ",
                                    vpdStream.str());

                                createPEL(additionalData,
                                          constants::PelSeverity::WARNING,
                                          constants::errIntfForInvalidVPD,
                                          nullptr);
                            }
                        }

                        // If cache data is not blank, then irrespective of
                        // hardware data(blank or other than cache), copy the
                        // cache data to vpd map as we don't need to change the
                        // cache data in either case in the process of
                        // restoring system vpd.
                        types::Binary busData(busValue.begin(), busValue.end());
                        kwdValue = busValue;
                    }
                    else if (kwdValue.find_first_not_of(defaultValue) ==
                             std::string::npos)
                    {
                        if (recordName == "VSYS" && keyword == "FV")
                        {
                            // Continue to the next keyword without logging +PEL
                            // for VSYS FV(stores min version of BMC firmware).
                            // Reason:There is a requirement to support blank FV
                            // so that customer can use the system without
                            // upgrading BMC to the minimum required version.
                            continue;
                        }
                        std::string errMsg = "VPD is blank on both cache and "
                                             "hardware for record: ";
                        errMsg += (*it).first;
                        errMsg += " and keyword: ";
                        errMsg += keyword;
                        errMsg += ". SSR need to update hardware VPD.";

                        // both the data are blanks, log PEL
                        types::PelAdditionalData additionalData;
                        additionalData.emplace("CALLOUT_INVENTORY_PATH",
                                               objectPath);

                        additionalData.emplace("DESCRIPTION", errMsg);

                        // log PEL TODO: Block IPL
                        createPEL(additionalData, constants::PelSeverity::ERROR,
                                  constants::errIntfForBlankSystemVPD, nullptr);
                        continue;
                    }
                }
            }
        }
    }
}

void Parser::populateSystemVPDOnDbus(Parsed& vpdMap, const nlohmann::json& js)
{
    // Map of property and its value.
    types::PropertyMap prop;

    // Map of interface(key) and propertyMap(value) defined above.
    types::InterfaceMap interfaces;

    // Map of object path(key) and interfaceMap(value) defined above.
    types::ObjectMap objects;

    types::InterfaceList interfaceList = {constants::motherBoardInterface};
    // call mapper to check for object path creation
    types::MapperResponse subTree =
        getObjectSubtreeForInterfaces(constants::pimPath, 0, interfaceList);

    // Get motherboard inventory path from JSON.
    const auto mboardPath =
        js["frus"][constants::systemVpdFilePath].at(0).value("inventoryPath",
                                                             "");

    // If Mapper response shows motherboard path, we should attemp restoration
    // of system VPD.
    if ((subTree.size() != 0) &&
        (subTree.find(constants::pimPath + mboardPath) != subTree.end()))
    {
        restoreSystemVPD(vpdMap, mboardPath);
    }
    else
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Motherboard path not found, System VPD restoration "
            "not attempted.");
    }
}

void Parser::parseVPDData(const std::string& filePath, const nlohmann::json& js)
{
    try
    {
        types::Binary vpdVector = getVpdDataInVector(js, filePath);
        std::string baseFruInventoryPath =
            js["frus"][filePath][0]["inventoryPath"];
        ParserInterface* parser = ParserFactory::getParser(
            vpdVector, (constants::pimPath + baseFruInventoryPath));
        std::variant<types::KeywordVpdMap, Store> parseResult;
        parseResult = parser->parse();
        if (get_if<Store>(&parseResult))
        {
            std::cout << "data not null, delete this code before commit. added "
                         "to remove warning of unused variable"
                      << std::endl;
        }

        if (auto pVal = get_if<vpd::Store>(&parseResult))
        {
            if (filePath == constants::systemVPDFilePath)
            {
                populateSystemVPDOnDbus(pVal->getVpdMap(), js);
            }
        }

        /*    if (auto pVal = get_if<vpd::Store>(&parseResult))
            {
                //       populateDbus(pVal->getVpdMap(), js, file);
            }
            else if (auto pVal = get_if<KeywordVpdMap>(&parseResult))
            {
                //       populateDbus(*pVal, js, file);
            } */

        // release the parser object
        ParserFactory::freeParser(parser);
    }
    catch (const std::exception& e)
    {
        //    executePostFailAction(js, file);
        // throw;
    }
}

} // namespace vpd
} // namespace openpower