#pragma once

#include "impl.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

namespace openpower
{
namespace vpd
{
namespace memory
{
namespace parser
{
using ParserInterface = openpower::vpd::parser::interface::ParserInterface;
using kwdVpdMap = openpower::vpd::inventory::KeywordVpdMap;

class memoryVpdParser : public ParserInterface
{
  public:
    memoryVpdParser() = delete;
    memoryVpdParser(const memoryVpdParser&) = delete;
    memoryVpdParser& operator=(const memoryVpdParser&) = delete;
    memoryVpdParser(memoryVpdParser&&) = delete;
    memoryVpdParser& operator=(memoryVpdParser&&) = delete;
    ~memoryVpdParser() = default;

    /**
     * @brief Constructor
     *
     * Move memVpdVector to parser object's memVpdVector
     */
    memoryVpdParser(const Binary& VpdVector) : memVpd(VpdVector) {}

    /**
     * @brief Parse the memory VPD binary data.
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    std::variant<kwdVpdMap, Store> parse();

    /**
     * @brief An api to return interface name with respect to
     * publish data on cache
     *
     * @return - Interface name for that vpd type.
     */
    std::string getInterfaceName() const;

  private:
    /**
     * @brief An api to read keywords.
     *
     * @return- map of kwd:value
     */
    kwdVpdMap readKeywords(Binary::const_iterator iterator);

    /**
     * @brief This function calculates dimm size from DDIMM VPD
     *
     * @param[in] iterator - iterator to buffer containing VPD
     * @return calculated data or 0 in case of any error.
     */
    size_t getDDimmSize(Binary::const_iterator iterator);

    /**
     * @brief This function calculates DDR4 based DDIMM's capacity
     *
     * @param[in] iterator - iterator to buffer containing VPD
     * @return calculated data or 0 in case of any error.
     */
    auto getDdr4BasedDDimmSize(Binary::const_iterator iterator);

    /**
     * @brief This function calculates DDR5 based DDIMM's capacity
     *
     * @param[in] iterator - iterator to buffer containing VPD
     * @return calculated data or 0 in case of any error.
     */
    auto getDdr5BasedDDimmSize(Binary::const_iterator iterator);

    /**
     * @brief This function calculates DDR5 based die per package
     *
     * @param[in] l_ByteValue - the bit value for calculation
     * @return die per package value.
     */
    uint8_t getDDR5DiePerPackage(uint8_t l_ByteValue);

    /**
     * @brief This function calculates DDR5 based density per die
     *
     * @param[in] l_ByteValue - the bit value for calculation
     * @return density per die.
     */
    uint8_t getDDR5DensityPerDie(uint8_t l_ByteValue);

    /**
     * @brief This function checks the validity of the bits
     *
     * @param[in] l_ByteValue - the byte value with relevant bits
     * @param[in] shift - shifter value to selects needed bits
     * @param[in] minValue - minimum value it can contain
     * @param[in] maxValue - maximum value it can contain
     * @return true if valid else false.
     */
    bool checkValidValue(uint8_t l_ByteValue, uint8_t shift, uint8_t minValue,
                         uint8_t maxValue);

    // vdp file to be parsed
    const Binary& memVpd;
};
} // namespace parser
} // namespace memory
} // namespace vpd
} // namespace openpower
