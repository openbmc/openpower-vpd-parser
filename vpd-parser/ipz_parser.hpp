#pragma once

#include "const.hpp"
#include "parser_interface.hpp"
#include "store.hpp"
#include "types.hpp"

#include <vector>

namespace openpower
{
namespace vpd
{
namespace ipz
{
namespace parser
{

using ParserInterface = openpower::vpd::parser::interface::ParserInterface;
using kwdVpdMap = openpower::vpd::inventory::KeywordVpdMap;

class IpzVpdParser : public ParserInterface
{
  public:
    IpzVpdParser() = delete;
    IpzVpdParser(const IpzVpdParser&) = delete;
    IpzVpdParser& operator=(const IpzVpdParser&) = delete;
    IpzVpdParser(IpzVpdParser&&) = delete;
    IpzVpdParser& operator=(IpzVpdParser&&) = delete;
    ~IpzVpdParser() = default;

    /**
     * @brief Constructor
     */
    IpzVpdParser(Binary&& VpdVector) : vpd(std::move(VpdVector))
    {
    }

    /**
     * @brief Parse the memory VPD binary data.
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    std::variant<kwdVpdMap, Store> parse(std::string filePath);

    /**
     * @brief An api to return interface name with respect to
     * the parser selected.
     *
     * @return - Interface name for that vpd type.
     */
    std::string getInterfaceName() const;

    /** @brief API to check vpd header
     *  @param [in] vpd - VPDheader in binary format
     */
    void processHeader();

    /** @brief Fix ECC implementation
     *  Parse all the records in the vpd and check if the
     *  ECC is correct or incorrect or uncorrectable.
     *
     *  If the ecc is correct - no action needed.
     *  If the ecc is incorrect - correct the ecc and write back to the EEPROM.
     *  If the ecc is uncorrectable - Abort/Skip the ecc check based on the
     * record. For records present in VTOC - Skip the ecc check if the ecc is
     * uncorrectable. For all the other records - Abort the ecc check if the ecc
     * is uncorrectable.
     */
    Binary fixEcc();

  private:
    Binary vpd;
}; // class IpzVpdParser

} // namespace parser
} // namespace ipz
} // namespace vpd
} // namespace openpower
