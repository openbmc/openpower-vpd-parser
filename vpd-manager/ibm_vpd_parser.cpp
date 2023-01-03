#include "ibm_vpd_parser.hpp"

#include "ibm_vpd_utils.hpp"
#include "parser_factory.hpp"
#include "parser_interface.hpp"
#include "store.hpp"
#include "types.hpp"

namespace openpower
{
void VPDParser::parseVPDData(const std::string& filePath,
                             const nlohmann::json& js)
{
    try
    {
        vpd::Binary vpdVector = vpd::getVpdDataInVector(js, filePath);
        std::string baseFruInventoryPath =
            js["frus"][filePath][0]["inventoryPath"];
        vpd::parser::interface::ParserInterface* parser =
            vpd::parser::factory::ParserFactory::getParser(
                vpdVector, (vpd::constants::pimPath + baseFruInventoryPath));
        std::variant<vpd::inventory::KeywordVpdMap, vpd::Store> parseResult;
        parseResult = parser->parse();
        if (get_if<vpd::Store>(&parseResult))
        {
            std::cout << "data not null, delete this code before commit. added "
                         "to remove warning of unused variable"
                      << std::endl;
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
        vpd::parser::factory::ParserFactory::freeParser(parser);
    }
    catch (const std::exception& e)
    {
        //    executePostFailAction(js, file);
        // throw;
    }
}
} // namespace openpower