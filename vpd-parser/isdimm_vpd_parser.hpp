#pragma once

#include "impl.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

namespace openpower
{
namespace vpd
{
namespace isdimm
{
namespace parser
{
using ParserInterface = openpower::vpd::parser::interface::ParserInterface;
using kwdVpdMap = inventory::KeywordVpdMap;

class isdimmVpdParser : public ParserInterface
{
  public:
    isdimmVpdParser() = delete;
    isdimmVpdParser(const isdimmVpdParser&) = delete;
    isdimmVpdParser& operator=(const isdimmVpdParser&) = delete;
    isdimmVpdParser(isdimmVpdParser&&) = delete;
    isdimmVpdParser& operator=(isdimmVpdParser&&) = delete;
    ~isdimmVpdParser() = default;

    /**
     * @brief Constructor
     *
     * Move memVpdVector to parser object's memVpdVector
     */
    isdimmVpdParser(const Binary& VpdVector) : memVpd(VpdVector)
    {
    }

    /**
     * @brief Parse the memory SPD binary data.
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    std::variant<kwdVpdMap, Store> parse();

    /**
     * @brief An api to return interface name with respect to
     * published data on cache
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
    kwdVpdMap readKeywords(Binary::const_iterator& iterator);

    /**
     * @brief This function calculates DIMM size from SPD
     *
     * @param[in] iterator - iterator to buffer containing SPD
     * @return calculated data or 0 in case of any error.
     */
    auto getDDR4DimmCapacity(Binary::const_iterator& iterator);

    /**
     * @brief This function calculates part number from SPD
     *
     * @param[in] iterator - iterator to buffer containing SPD
     * @return calculated part number.
     */
    auto getDDR4PartNumber(Binary::const_iterator& iterator);

    /**
     * @brief This function calculates serial number from SPD
     *
     * @param[in] iterator - iterator to buffer containing SPD
     * @return calculated serial number.
     */
    auto getDDR4SerialNumber(Binary::const_iterator& iterator);

    /**
     * @brief This function allocates FRU number based on part number
     *
     * @param[in] partNumber - part number of the DIMM
     * @return allocated FRU number.
     */
    auto getDDR4FruNumber(const std::string& partNumber);

    /**
     * @brief This function allocates CCIN based on part number
     *
     * @param[in] partNumber - part number of the DIMM
     * @return allocated CCIN.
     */
    auto getDDR4CCIN(const std::string& partNumber);

    /**
     * @brief This function calculates DIMM size from SPD
     *
     * @param[in] iterator - iterator to buffer containing SPD
     * @return calculated data or 0 in case of any error.
     */
    auto getDDR5DimmCapacity(Binary::const_iterator& iterator);

    /**
     * @brief This function calculates part number from SPD
     *
     * @param[in] iterator - iterator to buffer containing SPD
     * @return calculated part number.
     */
    auto getDDR5PartNumber(Binary::const_iterator& iterator);

    /**
     * @brief This function calculates serial number from SPD
     *
     * @param[in] iterator - iterator to buffer containing SPD
     * @return calculated serial number.
     */
    auto getDDR5SerialNumber(Binary::const_iterator& iterator);

    /**
     * @brief This function allocates FRU number based on part number
     *
     * @param[in] partNumber - part number of the DIMM
     * @return allocated FRU number.
     */
    auto getDDR5FruNumber(const std::string& partNumber);

    /**
     * @brief This function allocates CCIN based on part number
     *
     * @param[in] partNumber - part number of the DIMM
     * @return allocated CCIN.
     */
    auto getDDR5CCIN(const std::string& partNumber);

    // vdp file to be parsed
    const Binary& memVpd;
};

inline std::string isdimmVpdParser::getInterfaceName() const
{
    return constants::memVpdInf;
}

} // namespace parser
} // namespace isdimm
} // namespace vpd
} // namespace openpower
