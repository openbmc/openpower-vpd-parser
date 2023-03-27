#pragma once
#include "parser_interface.hpp"
#include "types.hpp"

namespace openpower
{
namespace vpd
{
namespace parser
{
namespace factory
{
/** @class ParserFactory
 *  @brief Factory calss to instantiate concrete parser class.
 *
 *  This class should be used to instantiate an instance of parser class based
 *  on the typeof vpd file.
 */

class ParserFactory
{
  public:
    ParserFactory() = delete;
    ~ParserFactory() = delete;
    ParserFactory(const ParserFactory&) = delete;
    ParserFactory& operator=(const ParserFactory&) = delete;
    ParserFactory(ParserFactory&&) = delete;
    ParserFactory& operator=(ParserFactory&&) = delete;

    /**
     * @brief A method to get object of concrete parser class.
     * @param[in] - vpd file to check for the type.
     * @param[in] - InventoryPath to call out FRU in case PEL is logged.
     * @param[in] - vpdFilePath for VPD HW path.
     * @param[in] - vpdStartOffset for starting offset of VPD.
     * @return - Pointer to concrete parser class object.
     */
    static interface::ParserInterface*
        getParser(const Binary& vpdVector, const std::string& inventoryPath,
                  const std::string& vpdFilePath, uint32_t vpdStartOffset);

    /**
     * @brief A method to delete the parser object.
     * @param[in] - Pointer to the parser object.
     */
    static void freeParser(interface::ParserInterface* parser);
}; // ParserFactory

} // namespace factory
} // namespace parser
} // namespace vpd
} // namespace openpower