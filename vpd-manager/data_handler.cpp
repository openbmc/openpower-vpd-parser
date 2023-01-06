#include "data_handler.hpp"

#include "const.hpp"
#include "ibm_vpd_utils.hpp"
#include "parser_factory.hpp"
#include "parser_interface.hpp"

namespace openpower
{
namespace vpd
{
void parseVPDData(const std::string& filePath, const nlohmann::json& js,
                  types::parsedVPDMap& parsedData)
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

        // release parser object
        ParserFactory::freeParser(parser);

        if (auto pVal = get_if<Store>(&parseResult))
        {
            parsedData = pVal->getVpdMap();
        }
        else if (auto pVal = get_if<types::KeywordVpdMap>(&parseResult))
        {
            parsedData = *pVal;
        }
    }
    catch (const std::exception& e)
    {
    }
}
} // namespace vpd
} // namespace openpower