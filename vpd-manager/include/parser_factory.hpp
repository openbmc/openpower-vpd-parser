#pragma once

#include "logger.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

#include <memory>

namespace vpd
{
/**
 * @brief Factory calss to instantiate concrete parser class.
 *
 * This class should be used to instantiate an instance of parser class based
 * on the type of vpd file.
 */
class ParserFactory
{
  public:
    // Deleted APIs
    ParserFactory() = delete;
    ~ParserFactory() = delete;
    ParserFactory(const ParserFactory&) = delete;
    ParserFactory& operator=(const ParserFactory&) = delete;
    ParserFactory(ParserFactory&&) = delete;
    ParserFactory& operator=(ParserFactory&&) = delete;

    /**
     * @brief An API to get object of concrete parser class.
     *
     * The method is used to get object of respective parser class based on the
     * type of VPD extracted from VPD vector passed to the API.
     * To detect respective parser from VPD vector, add logic into the API
     * vpdTypeCheck.
     *
     * Note: API throws DataException in case vpd type check fails for any
     * unknown type. Caller responsibility to handle the exception.
     *
     * @param[in] i_vpdVector - vpd file content to check for the type.
     * @param[in] i_vpdFilePath - FRU EEPROM path.
     * @param[in] i_vpdStartOffset - Offset from where VPD starts in the VPD
     * file.
     *
     * @return - Pointer to concrete parser class object.
     */
    static std::shared_ptr<ParserInterface>
        getParser(const types::BinaryVector& i_vpdVector,
                  const std::string& i_vpdFilePath, size_t i_vpdStartOffset);
};
} // namespace vpd
