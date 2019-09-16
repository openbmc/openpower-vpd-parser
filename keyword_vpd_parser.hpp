#include "keyword_vpd_main.hpp"

#include <unordered_map>

namespace openpower
{
namespace keywordVpd
{
namespace parser
{
class KeywordVpdParser
{
  private:
    // Private attributes
    Binary::iterator checkSumStart;
    Binary::iterator checkSumEnd;
    Binary::iterator iterator;
    Binary keywordVpdVector;

    // Private methods
    void largeResourceIdentifierStringParser();

    std::unordered_map<std::string, std::vector<uint8_t>> kwValParser();

    void checkSumValidation();

    size_t getDataSize(Binary::iterator iterator);

  public:
    KeywordVpdParser(Binary kwVpdVector)
    {
        keywordVpdVector = kwVpdVector;
    }

    // Parser method
    std::unordered_map<std::string, std::vector<uint8_t>> kwVpdParser();
};
} // namespace parser
} // namespace keywordVpd
} // namespace openpower
