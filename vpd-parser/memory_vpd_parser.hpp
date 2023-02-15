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
     * @brief This function calculates DDR4 dimm size from DIMM VPD
     *
     * @param[in] iterator - iterator to buffer containing VPD
     * @return calculated data or 0 in case of any error.
     */
    auto getDDR4DimmSize(Binary::const_iterator iterator);

    // vdp file to be parsed
    const Binary& memVpd;

    /**
     * @brief This function calculates DDR5 dimm size from DIMM VPD
     *
     * @param[in] iterator - iterator to buffer containing VPD
     * @return calculated data or 0 in case of any error.
     */
    auto getDDR5DimmSize(Binary::const_iterator& iterator);

    /**
     * @brief This API calls the relevant APIs based on SDRAM type
     *
     * @param[in] iterator - iterator to buffer containing VPD
     * @return dimm size.
     */
    auto getDimmSize(Binary::const_iterator& iterator);
};

/**
 * @brief Get the count of even and odd numbers
 *
 * This function find the even & odd numbers count for
 * the n-1 range where 'n' should be greater than zero.
 *
 * @param num[in] - The given number
 * @param numOfEvenNumbrs[out] - The even numbers count
 * @param numOfOddNumbrs[out] - The odd numbers count
 */
void getEvenOddCount(uint8_t& num, uint8_t& numOfEvenNumbrs,
                     uint8_t& numOfOddNumbrs);

/**
 * @brief Get the SDRAM parameters
 *
 * This function extracts all the required parameters from the
 * SPD for Symmetrical module and Asymmetrical even ranks module
 *
 * @param iterator[in] - iterator - iterator to buffer containing SPD
 * @param sdramIoWidth[out] - The SDRAM I/O Width
 * @param diePerPkg[out] - The Die per Package
 * @param dramDnstyPerDie[out] - The SDRAM Density Per Die
 *
 * @return true if extracted sdram params are valid, false otherwise.
 */
auto getSdramParamsFromSPDData(Binary::const_iterator& iterator,
                               uint8_t& sdramIoWidth, uint8_t& diePerPkg,
                               uint8_t& sdramDnstyPerDie);
/**
 * @brief Get the SDRAM parameters
 *
 * This function extracts all the required parameters from
 * the SPD for Asymmetrical odd ranks module
 *
 * @param iterator[in] - iterator - iterator to buffer containing SPD
 * @param sdramIoWidth[out] - The SDRAM I/O Width
 * @param diePerPkg[out] - The Die per Package
 * @param dramDnstyPerDie[out] - The SDRAM Density Per Die
 *
 * @return true if extracted sdram params are valid, false otherwise.
 */
auto getSdramParamsForAsymmetricalModule(Binary::const_iterator& iterator,
                                         uint8_t& sdramIoWidth,
                                         uint8_t& diePerPkg,
                                         uint8_t& sdramDnstyPerDie);
} // namespace parser
} // namespace memory
} // namespace vpd
} // namespace openpower
