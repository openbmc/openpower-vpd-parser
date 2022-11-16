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
    IpzVpdParser(const Binary& VpdVector, const std::string& path,
                 const std::string& vpdPath, uint32_t offset) :
        vpd(VpdVector), inventoryPath(path), vpdPath(vpdPath), offset(offset)
    {
    }

    /**
     * @brief Parse the memory VPD binary data.
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    std::variant<kwdVpdMap, Store> parse();

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

  private:
    const Binary& vpd;

    /*Inventory path of the FRU */
    const std::string inventoryPath;

    /* VPD Path */
    const std::string vpdPath;

    /* Offset */
    uint32_t offset;

}; // class IpzVpdParser

} // namespace parser
} // namespace ipz
} // namespace vpd
} // namespace openpower